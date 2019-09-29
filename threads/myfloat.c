#include "myfloat.h"

void InitMyFloat(MyFloat* a, int integer, int precision){
    ASSERT(precision <= 31);
    ASSERT(precision >= 8);
    a->val = integer;
    a->precision = precision;
}

MyFloat* MySubstraction(MyFloat* a, MyFloat* b){
    a->val -= b->val;
    return a;
}

MyFloat* MyAdd(MyFloat* a, MyFloat* b){
    a->val += b->val;
    return a;
}

MyFloat* MyMultiply(MyFloat* a, MyFloat* b UNUSED){
    // TODO
    return a;
}
MyFloat* MyDivide(MyFloat* a,MyFloat* b UNUSED){
    // TODO
    return a;
}
MyFloat* MyMultiply_Int(MyFloat* a, int b){
    a->val *= b;
    return a;
}
MyFloat* MyDivide_Int(MyFloat* a, int b){
    a->val /= b;
    return a;
}
MyFloat* MyMultiply_Add(MyFloat* a, int b){
    // TODO boundary check
    a->val += (b << a->precision);
    return a;
}

MyFloat* MyDivide_Sub(MyFloat* a, int b){
    // TODO boundary check
    a->val -= (b << a->precision);
    return a;
}

int MyFloat2Int(MyFloat* a){
    return (a->val >> a->precision);
}

int MyFloat2Int_100(MyFloat* a){
    int integer_part = (a->val) >> (a->precision);
    int fraction_part = a->val - (integer_part << a->precision);
    return integer_part*100 + (fraction_part*100 >> (a->precision));
}