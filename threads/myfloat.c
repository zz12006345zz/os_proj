#include "myfloat.h"
#include <stdio.h>
void InitMyFloat(MyFloat* a, int integer, int precision){
    ASSERT(precision <= 31);
    ASSERT(precision >= 8);
    a->val = (integer << precision);
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
    // printf("multiply %d,%d\n",MyFloat2Int_100(a),MyFloat2Int_100(b));
    a->val = (int)(((int64_t)a->val * (int64_t)b->val) >> a->precision);
    // printf("%d\n",MyFloat2Int_100(a));
    return a;
}
MyFloat* MyDivide(MyFloat* a, const MyFloat* b){
    ASSERT(a->precision == b->precision);
    // printf("divide %d,%d\n",MyFloat2Int_100(a),MyFloat2Int_100(b));
    a->val = (int)(((int64_t)a->val << (a->precision))/b->val);
    // printf("%d\n",MyFloat2Int_100(a));
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
    if(a->val >= 0){
        return (a->val + (1 << a->precision)/2) >> (a->precision);
    }
    return (a->val + (1 << a->precision)/2) >> (a->precision);
}

int MyFloat2Int_100(const MyFloat* a){
    if(a->val >= 0){
        return ((int64_t)a->val *100 + (1 << a->precision)/2) >> (a->precision);
    }
    return ((int64_t)a->val *100 - (1 << a->precision)/2) >> (a->precision);
}