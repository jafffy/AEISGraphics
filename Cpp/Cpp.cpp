// Cpp.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <vector>

int main()
{
    std::vector<int> v;

    v.push_back(0);
    v.push_back(0);
    v.push_back(0);

    std::vector<int> v2 = v;
    v2.push_back(1);

    for (int i = 0; i < v2.size(); ++i) {
        printf("%d\n", v2[i]);
    }

    return 0;
}

