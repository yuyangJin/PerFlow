#include "utils.h"
#include <fstream>
#include <map>

void getFiles(std::string path, std::vector<std::string> &files) {
  DIR *dir;
  struct dirent *ptr;

  if ((dir = opendir(path.c_str())) == NULL) {
    ERR_EXIT("Open dir error...");
  }

  while ((ptr = readdir(dir)) != NULL) {
    if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0)
      continue;
    else if (ptr->d_type == 8)
      files.push_back(path + ptr->d_name);
    else if (ptr->d_type == 10)
      continue;
    else if (ptr->d_type == 4) {
      // files.push_back(ptr->d_name);
      getFiles(path + ptr->d_name + "/", files);
    }
  }
  closedir(dir);
}

void split(const string &str, const string &delim, vector<string> &res) {
  if ("" == str) return;
  // convert str from string to char*
  char *strs = new char[str.length() + 1];
  strcpy(strs, str.c_str());

  char *d = new char[delim.length() + 1];
  strcpy(d, delim.c_str());

  char *p = strtok(strs, d);
  while (p) {
    string s = p;      // convert splited p from char* to string
    res.push_back(s);  // store in res(result)
    p = strtok(NULL, d);
  }

  return;
}

void split(char *str, const char *delim, std::vector<std::string> &res) {
  char *s = strdup(str);
  char *token;
  if (strlen(s) == 0) {
    return;
  }

  int n = 0;
  for (token = strsep(&s, delim); token != NULL; token = strsep(&s, delim)) {
    std::string token_str = token;  // convert splited p from char* to string
    res.push_back(token_str);       // store in res(result)
                                    // strcpy(dst[n++], token);
  }
  return;
}
