#ifndef MYFLOAT
#define MYFLOAT
#include <debug.h>

/* ex: to define a 17.14 format float number
        set precision = 14
*/
typedef struct _MyFloat {
    int val;
    int precision; /* decimal fraction 2^(-precision)*/
}MyFloat;

void InitMyFloat(MyFloat* a,int integer, int precision);
MyFloat* MySubstraction(MyFloat* a, MyFloat* b);
MyFloat* MyAdd(MyFloat* a, MyFloat* b);
MyFloat* MyMultiply(MyFloat* a, MyFloat* b UNUSED);
MyFloat* MyDivide(MyFloat* a,MyFloat* b UNUSED);

MyFloat* MyMultiply_Int(MyFloat* a, int b);
MyFloat* MyDivide_Int(MyFloat* a, int b);
MyFloat* MyMultiply_Add(MyFloat* a, int b);
MyFloat* MyDivide_Sub(MyFloat* a, int b);
int MyFloat2Int(MyFloat* a);
int MyFloat2Int_100(MyFloat* a);
#endif