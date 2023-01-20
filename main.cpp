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

string inputfname, newfname, line;
vector<string> vars[64];
int scope = 0, balance = 0, startIndex, templateEmbedDepth = 0;
bool inString = false, inTemplateString = false, inComment = false, inDefine[64] = {false}, inAssignment, hasArgs, functionAssigned = false;

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

      // Don't continue parsing if we're in a comment
      if (inComment) continue;

      // Concatenate template literals, don't continue after
      if (line[i] == '`') {
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

      // Check if we're in a comment
      if (line[i] == '/' && line[i+1] == '/') break;
      if (line[i] == '/' && line[i+1] == '*') { inComment = true;  i ++; continue; }
      if (line[i] == '*' && line[i+1] == '/') { inComment = false; i ++; continue; }

      // Close variable definition set upon semicolon
      if (line[i] == ';') { inDefine[scope] = false; continue; }

      // Calculate current scope by counting brackets
      if (line[i] == '{') {
        scope ++;
        continue;
      }
      // Closing a block means never returning to it, so we clear all current scope specific values
      if (line[i] == '}') {
        inDefine[scope] = false;
        vars[scope].clear();
        scope --;
        continue;
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

        inAssignment = true;
        balance = 0;

        // Change '=' to the slot assignment operator '<-'
        while (i < line.length()) {

          i ++;
          if (!inAssignment) {

            if (line[i] == '(' || line[i] == '{') balance ++;
            else if (line[i] == ')' || line[i] == '}') balance --;
            else if (line[i] == ',' && balance == 0) inAssignment = true;

          } else if (line[i] == '=') {

            line[i] = '-';
            line.insert(i++, "<");
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

        functionAssigned = false;
        for (int j = i - 1; j >= 0; j --) {
          if (line[j] == '=') {
            functionAssigned = true;
            break;
          }
          if (isVarChar(line[j])) break;
        }

        for (startIndex = i + 8; line[startIndex] != '('; startIndex ++);

        balance = 1;
        hasArgs = false;

        for (i = startIndex + 1; balance != 0; i ++) {

          if (line[i] == '(') balance ++;
          else if (line[i] == ')') balance --;

          // Find variables in the function definition
          if (isVarChar(line[i])) {
            hasArgs = true;
            if (!isVarChar(line[i - 1])) startIndex = i;
          } else {
            if (isVarChar(line[i - 1])) vars[scope + 1].push_back(line.substr(startIndex, i - startIndex));
          }

        }

        i --;
        for (int j = 0; j <= scope; j ++) {

          for (int k = 0; k < vars[j].size(); k ++) {

            if (j == scope && functionAssigned && k == vars[j].size() - 1) continue;

            if (hasArgs || k > 0) {
              hasArgs = true;
              line.insert(i, ",");
              i ++;
            }

            line.insert(i, vars[j][k]);
            i += vars[j][k].length();
            line.insert(i, "=");
            i ++;
            line.insert(i, vars[j][k]);
            i += vars[j][k].length();

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
