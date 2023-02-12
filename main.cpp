#include <iostream>
#include <fstream>
#include <string>
#include <vector>

using namespace std;

bool isVarChar (char c) {
  if (c >= 48 && c <= 57) return true; // Digits 0-9
  if (c >= 65 && c <= 90) return true; // Letters A-Z
  if (c >= 97 && c <= 122) return true; // Letters a-z
  if (c == 95) return true; // Underscore
  return false;
}

string inputfname, newfname, line, tmparg;
vector<pair <string, bool>> vars[64], functionArgs;
int scope = 0, balance = 0, lineNumber = 0, startIndex, endIndex, templateEmbedDepth = 0, inDefineBalance[64] = {0}, functionAssigned = 0, functionArgsCount, freeVarStart = -1, forDefineBalance = 0;
bool inString = false, inTemplateString = false, inComment = false, inDefine[64] = {false}, lookForDefine = false, inAssignment, functionHasName, functionRemovedArg, functionHasArgs, debugMode = false;

int main (const int argc, char *argv[]) {

  if (argc < 2) {
    cout << "Enter file path: ";
    getline(cin, inputfname);
  } else {
    inputfname = argv[1];
  }

  newfname = inputfname;
  newfname.erase(newfname.length() - 4, 1);

  ifstream input(inputfname);
  ofstream output(newfname);

  if (!input.is_open()) {
    cout << "File \"" << inputfname << "\" could not be opened.\n";
    input.close();
    output.close();
    return 1;
  }

  while (getline(input, line)) {

    lineNumber ++;
    if (line == "//#DEBUG") {
      lineNumber ++;
      debugMode = !debugMode;
      continue;
    }

    for (int i = 0; i < line.length(); i ++) {

      // Skip escaped characters
      if (line[i] == '\\') { i ++; continue; }

      // Concatenate template literals, don't continue after
      if (line[i] == '`' && !inComment) {
        if (templateEmbedDepth == 0) inTemplateString = !inTemplateString;
        line[i] = '"';
        continue;
      }

      if (inTemplateString) {

        if (line[i] == '"') {
          line.insert(i++, "\\");
          continue;
        }

        if (line[i] == '$' && line[i + 1] == '{') {
          templateEmbedDepth ++;
          line.erase(i, 2);
          line.insert(i, "\"+(");
          i += 2;
          continue;
        }

        if (line[i] == '}') {
          templateEmbedDepth --;
          line.erase(i, 1);
          line.insert(i, ")+\"");
          i += 2;
          continue;
        }

        continue;

      }

      // Don't continue parsing if we're in a string
      if (line[i] == '"') { inString = !inString; continue; }
      if (inString) continue;

      // Don't continue parsing if we're in a comment
      if (line[i] == '/' && line[i+1] == '/') break;
      if (line[i] == '/' && line[i+1] == '*') { inComment = true;  i ++; continue; }
      if (line[i] == '*' && line[i+1] == '/') { inComment = false; i ++; continue; }
      if (inComment) continue;

      // Close variable definition set upon semicolon
      if (line[i] == ';') {
        inDefine[scope] = false;
        lookForDefine = false;
        continue;
      }

      // Calculate current scope by counting brackets
      if (line[i] == '{') {
        scope ++;
        continue;
      }
      // Closing a block means never returning to it, so we clear all current scope specific values
      if (line[i] == '}') {
        inDefine[scope] = false;
        inDefineBalance[scope] = 0;
        lookForDefine = false;
        vars[scope].clear();
        scope --;
        continue;
      }

      // Check for key assignments in objects
      if (line[i] == ':') {

        if (i == freeVarStart) {
          freeVarStart = -1;
          continue;
        }
        
        line[i] = '=';
        continue;

      }

      // Keep track of bracket balance while defining variables
      if (inDefine[scope]) {

        if (line[i] == '(' || line[i] == '[') {
          inDefineBalance[scope] ++;
          continue;
        }
        if (line[i] == ')' || line[i] == ']') {
          inDefineBalance[scope] --;
          continue;
        }

      }

      // Find the end of for loops
      if (forDefineBalance != 0) {

        if (line[i] == '(') {
          forDefineBalance ++;
          continue;
        }
        if (line[i] == ')') {
          forDefineBalance --;
          continue;
        }

      }

      // Find variables defined using the "let" keyword
      if (!inDefine[scope] && line.compare(i, 4, "let ") == 0 && (i == 0 || !isVarChar(line[i - 1]))) {

        // Replace "let" with "local"
        line.erase(i, 3);
        line.insert(i, "local");

        // Don't store the variable if we're in a for loop
        if (forDefineBalance != 0) continue;

        inDefine[scope] = true;

        for (startIndex = i + 6; !isVarChar(line[startIndex]); startIndex ++);
        for (i = startIndex + 1; isVarChar(line[i]); i ++);

        vars[scope].push_back(make_pair(line.substr(startIndex, i - startIndex), false));
        i --;
        continue;

      }

      // Find globals defined using the "var" keyword
      if (!inDefine[scope] && line.compare(i, 4, "var ") == 0 && (i == 0 || !isVarChar(line[i - 1]))) {

        // Remove "var" keyword
        line.erase(i, 4);

        inAssignment = true;
        balance = 0;

        // Change '=' to the slot assignment operator '<-'
        for (int j = i + 1; j < line.length(); j ++) {

          if (!inAssignment) {

            if (line[j] == '(' || line[j] == '{' || line[j] == '[') balance ++;
            else if (line[j] == ')' || line[j] == '}' || line[j] == ']') balance --;
            else if (line[j] == ',' && balance == 0) inAssignment = true;

          } else if (line[j] == '=') {

            line[j] = '-';
            line.insert(j++, "<");
            inAssignment = false;

          }

        }

        continue;

      }

      // Continue looking for variables if new ones could potentially be defined.
      if (inDefine[scope] && inDefineBalance[scope] == 0 && (line[i] == ',' || lookForDefine)) {

        // Continue looking for further variables on the next line
        if (i == line.length() - 1) {
          lookForDefine = true;
          break;
        }

        for (startIndex = i + 1; !isVarChar(line[startIndex]); startIndex ++);
        for (i = startIndex + 1; isVarChar(line[i]); i ++);

        vars[scope].push_back(make_pair(line.substr(startIndex, i - startIndex), false));
        lookForDefine = false;
        i --;
        continue;

      }

      // Apply found variables to functions within the same scope
      if (line.compare(i, 8, "function") == 0 && (i == 0 || !isVarChar(line[i - 1])) && !isVarChar(line[i + 8])) {

        functionHasName = false;
        endIndex = i + 8;

        for (int j = i - 1; j >= 0; j --) {
          if (line[j] == '=') {
            vars[scope][vars[scope].size() - 1].second = true;
            functionAssigned ++;
            break;
          }
          if (line[j] == ';') break;
        }

        for (startIndex = endIndex; startIndex < line.length(); startIndex ++) {

          if (!functionHasName) {
            if (isVarChar(line[startIndex])) functionHasName = true;
            endIndex = startIndex;
          } else if (!isVarChar(line[startIndex]) && functionAssigned == 0) {
            functionAssigned = 1;
            vars[scope].push_back(make_pair(line.substr(endIndex, startIndex - endIndex), true));
          }

          if (line[startIndex] == '(') break;

        }

        balance = 1;
        functionArgsCount = 0;
        functionHasArgs = false;

        for (endIndex = startIndex + 1; balance != 0; endIndex ++) {

          if (line[endIndex] == '(') balance ++;
          else if (line[endIndex] == ')') balance --;

          // Find variables in the function definition
          if (isVarChar(line[endIndex])) {
            functionArgsCount ++;
            if (!isVarChar(line[endIndex - 1])) startIndex = endIndex;
          } else {
            tmparg = line.substr(startIndex, endIndex - startIndex);
            functionArgs.push_back(make_pair(tmparg, false));
            if (isVarChar(line[endIndex - 1])) vars[scope + 1].push_back(make_pair(tmparg, false));
          }

        }

        line.insert(endIndex, ":(");
        freeVarStart = endIndex;
        endIndex += 2;

        for (int j = 0; j <= scope; j ++) {

          for (int k = 0; k < vars[j].size(); k ++) {

            // Check if we're including the function istelf
            if (vars[j][k].second) continue;

            // Check if the function already accepts some arguments
            if (functionArgsCount > 0) {

              // Check if any of the existing arguments overlap with variables from above scopes
              functionRemovedArg = false;

              for (int l = 0; l < functionArgs.size(); l ++) {
                if (vars[j][k] == functionArgs[l]) {
                  functionArgs.erase(functionArgs.begin() + l);
                  functionRemovedArg = true;
                  functionArgsCount --;
                  break;
                }
              }

              if (functionRemovedArg) continue;

            }
            
            // Insert a comma if there's an argument before this
            if (functionHasArgs) {
              line.insert(endIndex, ",");
              endIndex ++;
            } else functionHasArgs = true;
            
            // Simulate closure by adding free variables
            line.insert(endIndex, vars[j][k].first);
            endIndex += vars[j][k].first.length();

          }

        }

        line.insert(endIndex, ")");

        continue;

      }

      // Don't keep track of locals defined in for loops
      if (line.compare(i, 3, "for") == 0 && (i == 0 || !isVarChar(line[i - 1])) && !isVarChar(line[i + 3])) {

        while (line[i] != '(') i ++;
        forDefineBalance = 1;
        
        continue;

      }

    }

    if (inDefine[scope] && !inTemplateString) {

      for (int j = line.length() - 1; j >= 0; j --) {
        if (line[j] == ' ') continue;
        if (line[j] != ',') {
          inDefine[scope] = false;
          lookForDefine = false;
        }
        break;
      }

    }

    if (debugMode && !inTemplateString && !inDefine[scope] && !lookForDefine && !inComment) {
      output << "printl(\"peanut debug reached " << lineNumber << "\");\n";
    }

    if (inTemplateString) output << line << "\\n";
    else output << line << '\n';

  }

  input.close();
  output.close();

  return 0;

}
