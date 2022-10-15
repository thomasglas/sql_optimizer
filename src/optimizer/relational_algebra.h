#ifndef relational_algebra
#define relational_algebra

#include <stdio.h>
#include <vector>
#include <string>

typedef enum {
    RA__NODE__ROOT = 0,
    RA__NODE__SELECTION = 1,
    RA__NODE__PROJECTION = 2,
    RA__NODE__CROSS_PRODUCT = 3,
    RA__NODE__RELATION = 4,
    RA__NODE__GROUP_BY = 5,
    RA__NODE__EXPRESSION = 6,
    RA__NODE__RENAME = 7,
    RA__NODE__CONST = 8,
    RA__NODE__ATTRIBUTE = 9,
    RA__NODE__BOOL_PREDICATE = 10,
    RA__NODE__PREDICATE = 11
} Ra__Node__NodeCase;

typedef enum {
    RA__ARITHMETIC_OPERATOR__PLUS = 0,
    RA__ARITHMETIC_OPERATOR__MINUS = 1,
    RA__ARITHMETIC_OPERATOR__MULTIPLY = 2,
    RA__ARITHMETIC_OPERATOR__DIVIDE = 3
} Ra__Arithmetic_Operator__OperatorCase;

typedef enum {
    RA__BOOL_OPERATOR__AND = 0,
    RA__BOOL_OPERATOR__OR = 1,
    RA__BOOL_OPERATOR__NOT = 2
} Ra__Boolean_Operator__OperatorCase;

typedef enum {
    RA__BINARY_OPERATOR__EQ = 0,
    RA__BINARY_OPERATOR__NEQ = 1,
    RA__BINARY_OPERATOR__GREATER = 2,
    RA__BINARY_OPERATOR__LESS = 3,
    RA__BINARY_OPERATOR__GREATER_EQ = 4,
    RA__BINARY_OPERATOR__LESS_EQ = 5
} Ra__Binary_Operator;

class Ra__Node;
class Ra__Node__Cross_Product;
class Ra__Node__Projection;
class Ra__Node__Selection;
class Ra__Node__Relation;
class Ra__Node__Rename;
class Ra__Node__Bool_Predicate;
class Ra__Node__Predicate;
class Ra__Node__Expression;
class Ra__Node__Attribute;
class Ra__Node__Constant;

class Ra__Node{
    public:
        Ra__Node__NodeCase node_case;
        std::vector<Ra__Node*> childNodes;
        std::vector<Ra__Node*>* parentNodes;
};

class Ra__Node__Cross_Product: public Ra__Node{
    public:
        Ra__Node__Cross_Product(){
            node_case = Ra__Node__NodeCase::RA__NODE__CROSS_PRODUCT;
        }
};

class Ra__Node__Projection: public Ra__Node {
    public:
        Ra__Node__Projection(){
            node_case = Ra__Node__NodeCase::RA__NODE__PROJECTION;
        }
        std::vector<Ra__Node__Expression*> expressions;
};

class Ra__Node__Selection: public Ra__Node {
    public:
        Ra__Node__Selection(){
            node_case = Ra__Node__NodeCase::RA__NODE__SELECTION;
        }
        Ra__Node* predicate; // Ra__Node__Bool_Predicate or Ra__Node__Predicate
};

class Ra__Node__Relation: public Ra__Node {
    public:
        Ra__Node__Relation(){
            node_case = Ra__Node__NodeCase::RA__NODE__SELECTION;
        }
        std::string name;
        std::string alias;
};

class Ra__Node__Rename: public Ra__Node {
    public:
        Ra__Node__Rename(){
            node_case = Ra__Node__NodeCase::RA__NODE__RENAME;
        }
        std::string old_to_string(){
            return old_alias.length()>0 ? old_alias+"."+old_name : old_name;
        }
        std::string new_to_string(){
            return new_alias.length()>0 ? new_alias+"."+new_name : new_name;
        }
        std::string old_alias;
        std::string old_name;
        std::string new_alias;
        std::string new_name;
};

class Ra__Node__Bool_Predicate: public Ra__Node {
    public:
        Ra__Node__Bool_Predicate(){
            node_case = Ra__Node__NodeCase::RA__NODE__BOOL_PREDICATE;
        }
        std::vector<Ra__Node*> args;
        Ra__Boolean_Operator__OperatorCase bool_operator;
};

class Ra__Node__Predicate: public Ra__Node {
    public:
        Ra__Node__Predicate();
        Ra__Node__Expression* left;
        Ra__Node__Expression* right;
        Ra__Binary_Operator binaryOperator;
};

class Ra__Node__Expression: public Ra__Node {
    public:
        Ra__Node__Expression(){
            node_case = Ra__Node__NodeCase::RA__NODE__EXPRESSION;
        }
        std::vector<Ra__Node*> consts_attributes;
        std::vector<Ra__Arithmetic_Operator__OperatorCase> operators;
};

class Ra__Node__Attribute: public Ra__Node {
    public:
        Ra__Node__Attribute(){
            node_case = Ra__Node__NodeCase::RA__NODE__ATTRIBUTE;
        }
        std::string to_string(){
            return alias.length()>0 ? alias+"."+name : name;
        }
        std::string name;
        std::string alias;
};

class Ra__Node__Constant: public Ra__Node  {
    public:
        Ra__Node__Constant(){
            node_case = Ra__Node__NodeCase::RA__NODE__CONST;
        }
        std::string to_string(){
            switch(dataType){
                case 0: return std::to_string(i);
                case 1: return std::to_string(f);
                case 2: return str;
            }
        }

        int i;
        float f;
        std::string str;
        int dataType; //0:i, 1:f, 2:str
};

#endif