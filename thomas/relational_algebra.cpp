#include "relational_algebra.h"

std::string PredicateLeaf::toString(){
    if(isConst){
        switch(constValue->dataType){
            case 0: return std::to_string(constValue->i);
            case 1: return std::to_string(constValue->f);
            case 2: return constValue->str;
            default: return "error";
        }
    }
    else{
        std::string str = "";
        for(auto s: attr->attribute){
            str += s + ".";
        }
        str.pop_back();
        return str;
    }
}