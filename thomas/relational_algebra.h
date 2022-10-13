#include <stdio.h>
#include <vector>
#include <string>

class Attribute;
class Predicate;
class Constant;

class RaNode{
};

class Projection: public RaNode {
    public:
        std::vector<Attribute*> attributes;
        RaNode* innerNode;
};

class Selection: public RaNode {
    public:
        Predicate* p;
        RaNode* innerNode;
};

class Relations: public RaNode {
    public:
        Relations(std::vector<std::vector<std::string>> _relations)
        :relations(_relations)
        {}
        std::vector<std::vector<std::string>> relations;
};

class PredicateLeaf{
    public:
        Attribute* attr;
        Constant* constValue;
        bool isConst=false;

        PredicateLeaf(Attribute* _attr)
        :attr(_attr), isConst(false)
        {}

        PredicateLeaf(Constant* _constValue)
        :constValue(_constValue), isConst(true)
        {}

        std::string toString();
};

class Predicate {
    public:
        PredicateLeaf* left;
        PredicateLeaf* right;
        std::string binaryOperator;
};

class Attribute {
    public:
        std::vector<std::string> attribute;
};

class Constant {
    public:
        Constant(int _i)
        :i(_i), dataType(0)
        {}
        
        Constant(float _f)
        :f(_f), dataType(1)
        {}
        
        Constant(std::string _str)
        :str(_str), dataType(2)
        {}

        int i;
        float f;
        std::string str;
        int dataType;
};

