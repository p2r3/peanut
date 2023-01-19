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

string newfname, line;
vector<string> vars[64];
int scope = 0, balance = 0, prevBalance = 0, startIndex, endIndex;
bool inString = false, inTemplateString = false, inComment = false, inDefine[64] = {false}, hasArgs = false;

int main (const int argc, char *argv[]) {

  if (argc < 2) {
    cout << "No file was provided.\n";
    return 1;
  }

  newfname = argv[1];
  newfname.erase(newfname.length() - 4, 1);

  ifstream input(argv[1]);
  ofstream output(newfname);

  if (!input.is_open()) {
    cout << "File \"" << argv[1] << " could not be opened.\n";
    input.close();
    output.close();
    return 1;
  }

  while (getline(input, line)) {

    inComment = false;
    inDefine[scope] = false;

    for (int i = 0; i < line.length(); i ++) {

      // Skip escaped characters
      if (line[i] == '\\') { i ++; continue; }

      // Don't continue parsing if we're in a comment
      if (line[i] == '/' && line[i+1] == '/') { inComment = true;  i ++; continue; }
      if (line[i] == '/' && line[i+1] == '*') { inComment = true;  i ++; continue; }
      if (line[i] == '*' && line[i+1] == '/') { inComment = false; i ++; continue; }
      if (inComment) continue;

      // Concatenate template string literals, don't continue after
      if (line[i] == '`') {
        inTemplateString = !inTemplateString;
        line[i] = '"';
        continue;
      }

      if (inTemplateString) {

        if (line[i] == '"') {
          line.insert(i++, "\\");
          continue;
        }

        if (line[i] == '$' && line[i + 1] == '{') {
          line[i] = '"';
          line[++i] = '+';
          continue;
        }
        
        if (line[i] == '}') {
          line[i] = '+';
          line.insert(++i, "\"");
          continue;
        }

        continue;

      }

      // Don't continue parsing if we're in a string
      if (line[i] == '"') { inString = !inString; continue; }
      if (inString) continue;

      // Close variable definition set
      if (line[i] == ';') { inDefine[scope] = false; continue; }

      // Calculate current scope by counting brackets
      if (line[i] == '{') { scope ++; continue; }
      if (line[i] == '}') { inDefine[scope] = false; scope --; continue; }

      // Calculate bracket balance
      if (line[i] == '(') { balance ++; continue; }
      if (line[i] == ')') { balance --; continue; }

      // Find variables defined using the "let" keyword
      if (!inDefine[scope] && line.compare(i, 4, "let ") == 0 && (i == 0 || !isVarChar(line[i - 1]))) {

        // Replace "let" with "local"
        line.erase(i, 3);
        line.insert(i, "local");

        inDefine[scope] = true;

        for (startIndex = i + 6; !isVarChar(line[startIndex]); startIndex ++);
        for (endIndex = startIndex + 1; isVarChar(line[endIndex]); endIndex ++);

        vars[scope].push_back(line.substr(startIndex, endIndex - startIndex));
        i = endIndex;
        continue;

      }

      // Find globals defined using the "var" keyword
      if (!inDefine[scope] && line.compare(i, 4, "var ") == 0 && (i == 0 || !isVarChar(line[i - 1]))) {
      
        // Remove "var" keyword
        line.erase(i, 4);

        prevBalance = balance;

        for (int j = i; j < line.length(); j ++) {

          if (line[j] == '(') balance ++;
          else if (line[j] == ')') balance --;
          else if (line[j] == '=' && balance == prevBalance) {
            line[j] = '-';
            line.insert(j++, "<");
          }

        }

        continue;

      }

      // Continue looking for variables if new ones could potentially be defined.
      if (inDefine[scope] && line[i] == ',') {

        for (startIndex = i + 1; !isVarChar(line[startIndex]); startIndex ++);
        for (endIndex = startIndex + 1; isVarChar(line[endIndex]); endIndex ++);

        vars[scope].push_back(line.substr(startIndex, endIndex - startIndex));
        i = endIndex;
        continue;

      }

      // Apply found variables to functions within the same scope
      if (line.compare(i, 8, "function") == 0 && (i == 0 || !isVarChar(line[i - 1])) && !isVarChar(line[i + 8])) {

        for (startIndex = i + 8; line[startIndex] != '('; startIndex ++);

        prevBalance = balance;
        balance ++;
        hasArgs = false;

        for (endIndex = startIndex + 1; balance != prevBalance; endIndex ++) {
          
          if (line[endIndex] == '(') balance ++;
          else if (line[endIndex] == ')') balance --;

          // Find variables in the function definition
          if (isVarChar(line[endIndex])) {
            hasArgs = true;
            if (!isVarChar(line[endIndex - 1])) startIndex = endIndex;
          } else {
            if (isVarChar(line[endIndex - 1])) vars[scope + 1].push_back(line.substr(startIndex, endIndex - startIndex));
          }

        }

        endIndex --;
        for (int j = 0; j < vars[scope].size(); j ++) {
          
          if (j > 0 || hasArgs) {
            line.insert(endIndex, ",");
            endIndex ++;
          }

          line.insert(endIndex, vars[scope][j]);
          endIndex += vars[scope][j].length();
          line.insert(endIndex, "=");
          endIndex ++;
          line.insert(endIndex, vars[scope][j]);
          endIndex += vars[scope][j].length();

        }

        i = endIndex;
        continue;

      }

    }

    output << line << '\n';

  }

  input.close();
  output.close();

  return 0;

}
