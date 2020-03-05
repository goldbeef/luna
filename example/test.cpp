//
// Created by Heng Zhao on 2020-03-04.
//


#include <iostream>
using namespace std;

#define HELLO(content)  \
    cout << #content << endl



int main() {
      HELLO(test);
      return 0;
}
