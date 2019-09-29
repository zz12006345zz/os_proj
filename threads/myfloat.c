#include "myfloat.h"

void InitMyFloat(MyFloat* a, int integer, int precision){
    ASSERT(precision <= 31);
    ASSERT(precision >= 8);
    a->val = integer;
    a->precision = precision;
}
void CopyMyFloat(MyFloat* a, const MyFloat *b){
    a->precision = b->precision;
    a->val = b->val;
}

MyFloat* MySubstraction(MyFloat* a, MyFloat* b){
    a->val -= b->val;
    return a;
}

MyFloat* MyAdd(MyFloat* a, MyFloat* b){
    a->val += b->val;
    return a;
}

MyFloat* MyMultiply(MyFloat* a, const MyFloat* b){
    ASSERT(a->precision == b->precision);
    a->val = (int)(((int64_t)a->val * b->val) >> a->precision*2);
    return a;
}
MyFloat* MyDivide(MyFloat* a, const MyFloat* b){
    ASSERT(a->precision == b->precision);
    a->val = (int)(((int64_t)a->val << (a->precision*2))/b->val);
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
MyFloat* MyAdd_Int(MyFloat* a, int b){
    // TODO boundary check
    a->val += (b << a->precision);
    return a;
}

MyFloat* MySub_Int(MyFloat* a, int b){
    // TODO boundary check
    a->val -= (b << a->precision);
    return a;
}

int MyFloat2Int(const MyFloat* a){
    return (a->val >> a->precision);
}

int MyFloat2Int_100(const MyFloat* a){
    return (int64_t)a->val *100 >> (a->precision);
}