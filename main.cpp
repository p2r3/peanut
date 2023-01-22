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
vector<string> vars[64], functionArgs;
int scope = 0, balance = 0, startIndex, endIndex, templateEmbedDepth = 0, functionAssigned = 0, functionArgsCount;
bool inString = false, inTemplateString = false, inComment = false, inDefine[64] = {false}, inAssignment, functionHasName, functionRemovedArg, functionHasArgs;

// 0 - none, 1 - function, 2 - object
int bracketType[64] = {0};

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
    cout << "File \"" << inputfname << " could not be opened.\n";
    input.close();
    output.close();
    return 1;
  }

  while (getline(input, line)) {

    inDefine[scope] = false;

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
      if (line[i] == ';') { inDefine[scope] = false; continue; }

      if (!inDefine[scope] || bracketType[scope + 1] == 1) {

        // Calculate current scope by counting brackets
        if (line[i] == '{') {
          scope ++;
          continue;
        }
        // Closing a block means never returning to it, so we clear all current scope specific values
        if (line[i] == '}') {
          inDefine[scope] = false;
          vars[scope].clear();
          bracketType[scope] = 0;
          scope --;
          continue;
        }

      } else if (inDefine[scope]) {

        if (line[i] == '{') {
          scope ++;
          bracketType[scope] = 2;
          continue;
        }

        if (line[i] == '}') {
          bracketType[scope] = 0;
          scope --;
          continue;
        }

      }

      // Define behavior in objects
      if (bracketType[scope] == 2) {

        if (line[i] == ':') {
          line[i] = '=';
          inDefine[scope] = true;
          continue;
        }

        if (line[i] == ',') {
          inDefine[scope] = false;
          continue;
        }

      }

      // Find variables defined using the "let" keyword
      if (!inDefine[scope] && line.compare(i, 4, "let ") == 0 && (i == 0 || !isVarChar(line[i - 1]))) {

        // Replace "let" with "local"
        line.erase(i, 3);
        line.insert(i, "local");

        inDefine[scope] = true;

        for (startIndex = i + 6; !isVarChar(line[startIndex]); startIndex ++);
        for (i = startIndex + 1; isVarChar(line[i]); i ++);

        vars[scope].push_back(line.substr(startIndex, i - startIndex));
        continue;

      }

      // Find globals defined using the "var" keyword
      if (!inDefine[scope] && line.compare(i, 4, "var ") == 0 && (i == 0 || !isVarChar(line[i - 1]))) {

        // Remove "var" keyword
        line.erase(i, 4);

        inDefine[scope] = true;
        inAssignment = true;
        balance = 0;

        // Change '=' to the slot assignment operator '<-'
        for (int j = i + 1; j < line.length(); j ++) {

          if (!inAssignment) {

            if (line[j] == '(' || line[j] == '{') balance ++;
            else if (line[j] == ')' || line[j] == '}') balance --;
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
      if (inDefine[scope] && line[i] == ',') {

        for (startIndex = i + 1; !isVarChar(line[startIndex]); startIndex ++);
        for (i = startIndex + 1; isVarChar(line[i]); i ++);

        vars[scope].push_back(line.substr(startIndex, i - startIndex));
        continue;

      }

      // Apply found variables to functions within the same scope
      if (line.compare(i, 8, "function") == 0 && (i == 0 || !isVarChar(line[i - 1])) && !isVarChar(line[i + 8])) {

        bracketType[scope + 1] = 1;
        functionHasName = false;
        endIndex = i + 8;

        for (startIndex = endIndex; startIndex < line.length(); startIndex ++) {

          if (!functionHasName) {
            if (isVarChar(line[startIndex])) functionHasName = true;
            endIndex = startIndex;
          } else if (!isVarChar(line[startIndex]) && functionAssigned == 0) {
            functionAssigned = 1;
            vars[scope].push_back(line.substr(endIndex, startIndex - endIndex));
          }

          if (line[startIndex] == '(') break;

        }

        for (int j = i - 1; j >= 0; j --) {
          if (line[j] == '=') {
            functionAssigned ++;
            break;
          }
          if (isVarChar(line[j])) break;
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
            functionHasArgs = true;
            if (!isVarChar(line[endIndex - 1])) startIndex = endIndex;
          } else {
            tmparg = line.substr(startIndex, endIndex - startIndex);
            functionArgs.push_back(tmparg);
            if (isVarChar(line[endIndex - 1])) vars[scope + 1].push_back(tmparg);
          }

        }

        endIndex --;
        for (int j = 0; j <= scope; j ++) {

          for (int k = 0; k < vars[j].size(); k ++) {

            if (j == scope && k == vars[j].size() - functionAssigned) {
              functionAssigned --;
              continue;
            }

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
            
            // Inser a comma if there's an argument before this
            if (functionHasArgs) {
              line.insert(endIndex, ",");
              endIndex ++;
            } else functionHasArgs = true;
            
            // Simulate closure by adding predefined function arguments
            line.insert(endIndex, vars[j][k]);
            endIndex += vars[j][k].length();
            line.insert(endIndex, "=");
            endIndex++;
            line.insert(endIndex, vars[j][k]);
            endIndex += vars[j][k].length();

          }

        }

        continue;

      }

    }

    if (inTemplateString) output << line << "\\n";
    else output << line << '\n';

  }

  input.close();
  output.close();

  return 0;

}
