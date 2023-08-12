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
#include <unordered_map>
#include <string>
#include <vector>

#include "dbg.h"

#include "common.h"

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
void split(const string &str, const string &delim, vector<string> &res);
void split(char *str, const char *delim, std::vector<std::string> &res);

template <class keyType, class valueType>
void DumpMap(std::map<keyType, valueType> &m, std::string &file_name) {
  char file_name_str[MAX_STR_LEN];
  strcpy(file_name_str, file_name.c_str());
  std::ofstream fout;
  fout.open(file_name_str);

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

template <class keyType, class valueType>
void ReadHashMap(std::unordered_map<keyType, valueType> &m,
                 std::string &file_name) {
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
    ss >> std::hex >> key >> value;
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

#endif
