#ifndef relational_algebra
#define relational_algebra

#include <stdio.h>
#include <vector>
#include <string>
#include <cassert>

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
    RA__NODE__PREDICATE = 11,
    RA__NODE__FUNC_CALL = 12,
    RA__NODE__DUMMY = 13
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

typedef enum {
    RA__CONST_DATATYPE__INT = 0,
    RA__CONST_DATATYPE__FLOAT = 1,
    RA__CONST_DATATYPE__STRING = 2,
} Ra__Const_DataType;

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
        size_t n_children;
        virtual std::string to_string() {
            return "";
        };
        bool is_full(){
            return n_children == childNodes.size();
        }
};

class Ra__Node__Cross_Product: public Ra__Node{
    public:
        Ra__Node__Cross_Product(){
            node_case = Ra__Node__NodeCase::RA__NODE__CROSS_PRODUCT;
            n_children = 2;
        }
        std::string to_string(){
            assert(childNodes.size()==2);
            return "(" + childNodes[0]->to_string() + ")X(" + childNodes[1]->to_string() + ")";
        }
};

class Ra__Node__Projection: public Ra__Node {
    public:
        Ra__Node__Projection(){
            node_case = Ra__Node__NodeCase::RA__NODE__PROJECTION;
            n_children = 1;
        }
        std::string to_string(){
            assert(childNodes.size()==1);
            return "TT(" + childNodes[0]->to_string() + ")";
        }
        std::vector<Ra__Node__Expression*> expressions; //expression or func call
        std::string subquery_alias;
};

class Ra__Node__Selection: public Ra__Node {
    public:
        Ra__Node__Selection(){
            node_case = Ra__Node__NodeCase::RA__NODE__SELECTION;
            n_children = 1;
        }
        Ra__Node* predicate; // Ra__Node__Bool_Predicate or Ra__Node__Predicate
        std::string to_string(){
            assert(childNodes.size()==1);
            return "o(" + childNodes[0]->to_string() + ")";
        }
};

class Ra__Node__Relation: public Ra__Node {
    public:
        Ra__Node__Relation(){
            node_case = Ra__Node__NodeCase::RA__NODE__RELATION;
            n_children = 0;
        }
        std::string name;
        std::string alias;
        std::string to_string(){
            assert(childNodes.size()==0);
            return alias.length()==0 ? name : name + " " + alias;
        }
};

// class Ra__Node__Rename: public Ra__Node {
//     public:
//         Ra__Node__Rename(){
//             node_case = Ra__Node__NodeCase::RA__NODE__RENAME;
//         }
//         std::string old_to_string(){
//             return old_alias.length()>0 ? old_alias+"."+old_name : old_name;
//         }
//         std::string new_to_string(){
//             return new_alias.length()>0 ? new_alias+"."+new_name : new_name;
//         }
//         std::string old_alias;
//         std::string old_name;
//         std::string new_alias;
//         std::string new_name;
// };

class Ra__Node__Bool_Predicate: public Ra__Node {
    public:
        Ra__Node__Bool_Predicate(){
            node_case = Ra__Node__NodeCase::RA__NODE__BOOL_PREDICATE;
            n_children = 0;
        }
        std::vector<Ra__Node*> args;
        Ra__Boolean_Operator__OperatorCase bool_operator;
};

class Ra__Node__Predicate: public Ra__Node {
    public:
        Ra__Node__Predicate();
        Ra__Node__Expression* left;
        Ra__Node__Expression* right;
        // Ra__Binary_Operator binaryOperator;
        std::string binaryOperator;
};

class Ra__Node__Expression: public Ra__Node {
    public:
        Ra__Node__Expression(){
            node_case = Ra__Node__NodeCase::RA__NODE__EXPRESSION;
            n_children = 0;
        }
        std::vector<Ra__Node*> args; //const, attributes, func calls
        // std::vector<Ra__Arithmetic_Operator__OperatorCase> operators;
        std::vector<std::string> operators;
        std::string rename;
};

class Ra__Node__Attribute: public Ra__Node {
    public:
        Ra__Node__Attribute(){
            node_case = Ra__Node__NodeCase::RA__NODE__ATTRIBUTE;
            n_children = 0;
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
            n_children = 0;
        }
        std::string to_string(){
            switch(dataType){
                case RA__CONST_DATATYPE__INT: return std::to_string(i);
                case RA__CONST_DATATYPE__FLOAT: return f;
                case RA__CONST_DATATYPE__STRING: return "'" + str + "'";
            }
        }

        int i; // integer
        std::string f; // float
        std::string str;
        Ra__Const_DataType dataType;
};

class Ra__Node__Func_Call: public Ra__Node {
    public:
        Ra__Node__Func_Call(){
            node_case = Ra__Node__NodeCase::RA__NODE__FUNC_CALL;
            n_children = 0;
        }
    Ra__Node__Expression* expression;
    std::string func_name;
};

class Ra__Node__Dummy: public Ra__Node {
    public:
        Ra__Node__Dummy(){
            node_case = Ra__Node__NodeCase::RA__NODE__DUMMY;
            n_children = 0;
        }
};
#endif