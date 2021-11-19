#ifndef UTILS_H_
#define UTILS_H_

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "common.h"
#include "dbg.h"

#ifndef MAX_STR_LEN
#define MAX_STR_LEN 256
#endif

#ifndef MAX_CALL_PATH_LEN
#define MAX_CALL_PATH_LEN 1300
#endif

using namespace std;

void getFiles(std::string path, std::vector<std::string> &files);
// vector<string> split(const string &str, const string &delim);
// int split(char *str, const char *delim, char dst[][MAX_STR_LEN]);

template <class keyType, class valueType>
void DumpMap(std::map<keyType, valueType> &m, std::string &file_name) {
  char file_name_str[MAX_STR_LEN];
  strcpy(file_name_str, file_name.c_str());
  std::ofstream fout;
  fout.open(file_name_str);

  if (!fout.is_open()) {
    std::cout << "Failed to open" << file_name_str << std::endl;
  }

  for (auto &kv : m) {
    fout << kv.first << " " << kv.second << std::endl;
  }

  fout.close();
}

template <class keyType, class valueType>
void ReadMap(std::map<keyType, valueType> &m, std::string &file_name) {
  char file_name_str[MAX_STR_LEN];
  strcpy(file_name_str, file_name.c_str());
  // dbg(file_name_str);
  std::ifstream fin;
  fin.open(file_name_str);
  if (!fin.is_open()) {
    std::cout << "Failed to open" << file_name_str << std::endl;
  }

  keyType key;
  valueType value;

  std::string line;
  while (getline(fin, line)) {
    std::stringstream ss(line);
    ss >> key >> value;
    // dbg(key, value);
    m[key] = value;
  }

  // dbg(m.size());

  fin.close();
}

// template <class keyType, class valueType>
// void ReadMap(std::map<keyType, valueType>& m, std::string& file_name){
//   char file_name_str[MAX_STR_LEN];
//   strcpy(file_name_str, file_name.c_str());
//   std::ifstream fin;
//   fin.open(file_name_str);

//   char line[MAX_LINE_LEN];
//   //fin.getline(line, MAX_STR_LEN);
//   while (!(fin.eof())) {
//     // Read a line
//     fin.getline(line, MAX_STR_LEN);

//     char delim[] = " ";
//     // line_vec[4][MAX_STR_LEN];
//     std::vector<std::string> line_vec = split(line, delim);
//     int cnt = line_vec.size();
//     if (cnt == 2) {
//       keyType k = atoi(line_vec[0].c_str());
//       valueType v = std::string(line_vec[1]);
//       m[k] = v;
//     } else {
//       dbg(cnt);
//     }

//   }
//   fin.close();
// }

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

#endif
