//
// Created by gnilk on 26.03.26.
//

#include <stdio.h>

class MyClass {
public:
    MyClass() = default;
    virtual ~MyClass() = default;

    void MyFunc() {
        printf("This is one func\n");
    }
};


static void my_test_func() {
    MyClass myClass;
    myClass.MyFunc();
}

int main(int argc, const char *argv[]) {
    my_test_func();
    return 1;
}