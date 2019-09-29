#ifndef MYFLOAT
#define MYFLOAT
#include <debug.h>
#include <inttypes.h>
/* ex: to define a 17.14 format float number
        set precision = 14
*/
typedef struct _MyFloat {
    int val;
    int precision; /* decimal fraction 2^(-precision)*/
}MyFloat;

void InitMyFloat(MyFloat* a,int integer, int precision);
void CopyMyFloat(MyFloat* a, const MyFloat *b);

MyFloat* MySubstraction(MyFloat* a, MyFloat* b);
MyFloat* MyAdd(MyFloat* a, MyFloat* b);
MyFloat* MyMultiply(MyFloat* a, const MyFloat* b);
MyFloat* MyDivide(MyFloat* a, const MyFloat* b);

MyFloat* MyMultiply_Int(MyFloat* a, int b);
MyFloat* MyDivide_Int(MyFloat* a, int b);
MyFloat* MyAdd_Int(MyFloat* a, int b);
MyFloat* MySub_Int(MyFloat* a, int b);

int MyFloat2Int(const MyFloat* a);
int MyFloat2Int_100(const MyFloat* a);
#endif