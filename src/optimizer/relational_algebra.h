#ifndef relational_algebra
#define relational_algebra

#include <stdio.h>
#include <vector>
#include <string>
#include <cassert>
#include <iostream>

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
    RA__NODE__DUMMY = 13,
    RA__NODE__ORDER_BY = 14,
    RA__NODE__HAVING = 15,
    RA__NODE__JOIN = 16,
    RA__NODE__TYPE_CAST = 17,
    RA__NODE__SELECT_EXPRESSION = 18,
    RA__NODE__LIST = 19,
    RA__NODE__CASE_EXPR = 20,
    RA__NODE__CASE_WHEN = 21,
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
} Ra__Bool_Operator__OperatorCase;

typedef enum {
    RA__BINARY_OPERATOR__EQ = 0,
    RA__BINARY_OPERATOR__NEQ = 1,
    RA__BINARY_OPERATOR__GREATER = 2,
    RA__BINARY_OPERATOR__LESS = 3,
    RA__BINARY_OPERATOR__GREATER_EQ = 4,
    RA__BINARY_OPERATOR__LESS_EQ = 5
} Ra__Binary_Operator__OperatorCase;

typedef enum {
    RA__CONST_DATATYPE__INT = 0,
    RA__CONST_DATATYPE__FLOAT = 1,
    RA__CONST_DATATYPE__STRING = 2,
} Ra__Const_DataType__DataType;

typedef enum {
    RA__JOIN__INNER = 0,
    RA__JOIN__DEPENDENT_INNER_LEFT = 1,
    RA__JOIN__DEPENDENT_INNER_RIGHT = 2,
    RA__JOIN__LEFT = 3,
    RA__JOIN__RIGHT = 4,
    RA__JOIN__SEMI_LEFT = 5,
    RA__JOIN__SEMI_RIGHT = 6,
    RA__JOIN__SEMI_LEFT_DEPENDENT = 7,
    RA__JOIN__SEMI_RIGHT_DEPENDENT = 8,
    RA__JOIN__ANTI_LEFT = 9,
    RA__JOIN__ANTI_RIGHT = 10,
    RA__JOIN__ANTI_LEFT_DEPENDENT = 11,
    RA__JOIN__ANTI_RIGHT_DEPENDENT = 12,
} Ra__Join__JoinType;

typedef enum {
    RA__ORDER_BY__DEFAULT = 0,
    RA__ORDER_BY__ASC = 1,
    RA__ORDER_BY__DESC = 2,
} Ra__Order_By__SortDirection;

typedef enum {
    RA__TYPE_CAST__DATE = 0,
} Ra__Type_Cast__Type;

typedef enum {
    RA__FUNC_CALL__MIN = 0,
    RA__FUNC_CALL__MAX = 2,
    RA__FUNC_CALL__SUM = 3,
    RA__FUNC_CALL__COUNT = 4,
    RA__FUNC_CALL__AVG = 5,
} Ra__Func_Call__Func;

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
class Ra__Node__Group_By;
class Ra__Node__Order_By;
class Ra__Node__Having;
class Ra__Node__Select_Expression;
class Ra__Node__Case_When;
class Ra__Node__Case_Expr;

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

class Ra__Node__Join: public Ra__Node{
    public:
        Ra__Node__Join(Ra__Join__JoinType _type)
        : type(_type)
        {
            node_case = Ra__Node__NodeCase::RA__NODE__JOIN;
            n_children = 2;
            predicate = nullptr;
        }
        std::string to_string(){
            assert(this->is_full());
            std::string op;
            switch(type){
                case RA__JOIN__INNER: op = "J"; break;
                case RA__JOIN__LEFT: op = "LJ"; break;
                case RA__JOIN__RIGHT: op = "RJ"; break;
                case RA__JOIN__DEPENDENT_INNER_LEFT: op = "LDJ"; break;
                case RA__JOIN__DEPENDENT_INNER_RIGHT: op = "RDJ"; break;
                case RA__JOIN__SEMI_LEFT: op = "SLJ"; break;
                case RA__JOIN__SEMI_RIGHT: op = "SRJ"; break;
                case RA__JOIN__SEMI_LEFT_DEPENDENT: op = "SLDJ"; break;
                case RA__JOIN__SEMI_RIGHT_DEPENDENT: op = "SRDJ"; break;
                case RA__JOIN__ANTI_LEFT: op = "ALJ"; break;
                case RA__JOIN__ANTI_RIGHT: op = "ARJ"; break;
                case RA__JOIN__ANTI_LEFT_DEPENDENT: op = "ALDJ"; break;
                case RA__JOIN__ANTI_RIGHT_DEPENDENT: op = "ARDJ"; break;
                default: op = "join op not supported";
            }
            return "(" + childNodes[0]->to_string() + ")"+op+"(" + childNodes[1]->to_string() + ")";
        }
        std::string join_name(){
            switch(type){
                case RA__JOIN__INNER: return " join ";
                case RA__JOIN__LEFT: return " left join ";
                case RA__JOIN__RIGHT: return " right join ";
                default: return " join op not supported ";
            }
        }
        Ra__Node* predicate; // Ra__Node__Bool_Predicate/Ra__Node__Predicate
        Ra__Join__JoinType type;
        std::string alias;
        std::vector<std::string> columns;
};

class Ra__Node__Projection: public Ra__Node {
    public:
        Ra__Node__Projection(){
            node_case = Ra__Node__NodeCase::RA__NODE__PROJECTION;
            n_children = 1;
            distinct = false;
        }
        std::string to_string(){
            assert(childNodes.size()==1);
            return "\u03A0(" + childNodes[0]->to_string() + ")";
        }
        std::vector<Ra__Node*> args; // Ra__Node__Select_Expression
        std::string subquery_alias;
        std::vector<std::string> subquery_columns;
        bool distinct;
};

class Ra__Node__Selection: public Ra__Node {
    public:
        Ra__Node__Selection(){
            node_case = Ra__Node__NodeCase::RA__NODE__SELECTION;
            n_children = 1;
        }
        Ra__Node* predicate; // Ra__Node__Bool_Predicate/Ra__Node__Predicate
        std::string to_string(){
            assert(childNodes.size()==1);
            return "\u03C3(" + childNodes[0]->to_string() + ")";
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
        std::vector<Ra__Node__Attribute*> attributes;
        std::string to_string(){
            assert(childNodes.size()==0);
            return alias.length()==0 ? name : name + " " + alias;
        }
};

// sort operator
class Ra__Node__Order_By: public Ra__Node {
    public:
        Ra__Node__Order_By(){
            node_case = Ra__Node__NodeCase::RA__NODE__ORDER_BY;
            n_children = 1;
        }
        std::vector<Ra__Node*> args; // expressions/attr/const/case
        std::vector<Ra__Order_By__SortDirection> directions;
        std::string to_string(){
            assert(childNodes.size()==1);
            return "OB(" + childNodes[0]->to_string() + ")";
        }
};

class Ra__Node__Group_By: public Ra__Node {
    public:
        Ra__Node__Group_By(bool _implicit)
        :implicit(_implicit){
            node_case = Ra__Node__NodeCase::RA__NODE__GROUP_BY;
            n_children = 1;
        }
        std::vector<Ra__Node*> args; // expressions/attr/const/case
        bool implicit;
        std::string to_string(){
            assert(childNodes.size()==1);
            return "\u0393(" + childNodes[0]->to_string() + ")";
        }
};

class Ra__Node__Having: public Ra__Node {
    public:
        Ra__Node__Having(){
            node_case = Ra__Node__NodeCase::RA__NODE__HAVING;
            n_children = 1;
        }
        Ra__Node* predicate; // Ra__Node__Bool_Predicate or Ra__Node__Predicate
        std::string to_string(){
            assert(childNodes.size()==1);
            return "H(" + childNodes[0]->to_string() + ")";
        }
};

class Ra__Node__Bool_Predicate: public Ra__Node {
    public:
        Ra__Node__Bool_Predicate(){
            node_case = Ra__Node__NodeCase::RA__NODE__BOOL_PREDICATE;
            n_children = 0;
        }
        std::vector<Ra__Node*> args;
        Ra__Bool_Operator__OperatorCase bool_operator;
};

class Ra__Node__Predicate: public Ra__Node {
    public:
        Ra__Node__Predicate();
        Ra__Node* left;
        Ra__Node* right;
        // Ra__Binary_Operator binaryOperator;
        std::string binaryOperator;
};

class Ra__Node__Select_Expression: public Ra__Node {
    public:
        Ra__Node__Select_Expression();
        Ra__Node* expression;
        std::string rename;
};

// Todo: change to left and right expression and operator (one expr can be nullptr)
class Ra__Node__Expression: public Ra__Node {
    public:
        Ra__Node__Expression(){
            node_case = Ra__Node__NodeCase::RA__NODE__EXPRESSION;
            n_children = 0;
            r_arg = nullptr;
            l_arg = nullptr;
        }
        void add_arg(Ra__Node* arg){
            if(r_arg==nullptr) r_arg=arg;
            else if(l_arg==nullptr) l_arg=arg;
            else std::cout << "error in expression add arg, already full" << std::endl;
        }
        Ra__Node* r_arg;
        Ra__Node* l_arg; //const, attributes, func calls, type cast
        std::string operator_;
};
// class Ra__Node__Expression: public Ra__Node {
//     public:
//         Ra__Node__Expression(){
//             node_case = Ra__Node__NodeCase::RA__NODE__EXPRESSION;
//             n_children = 0;
//         }
//         std::vector<Ra__Node*> args; //const, attributes, func calls, type cast
//         std::vector<std::string> operators;
//         std::string rename;
// };

class Ra__Node__Type_Cast: public Ra__Node {
    public:
        Ra__Node__Type_Cast(){
            node_case = Ra__Node__NodeCase::RA__NODE__TYPE_CAST;
            n_children = 0;
            expression = new Ra__Node__Expression();
        }
        std::string to_string(){
            return "";
        }
        Ra__Node* expression;
        std::string type;
        std::string typ_mod; // interval '2' month
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
                default: return "tostring error";
            }
        }

        int i; // integer
        std::string f; // float
        std::string str;
        Ra__Const_DataType__DataType dataType;
};

class Ra__Node__Func_Call: public Ra__Node {
    public:
        Ra__Node__Func_Call(){
            node_case = Ra__Node__NodeCase::RA__NODE__FUNC_CALL;
            n_children = 0;
            is_aggregating = false;
            agg_distinct = false;
        }
    std::vector<Ra__Node*> args;
    std::string func_name;
    bool is_aggregating;
    bool agg_distinct;
};

class Ra__Node__List: public Ra__Node {
    public:
        Ra__Node__List(){
            node_case = Ra__Node__NodeCase::RA__NODE__LIST;
            n_children = 0;
        }
    std::vector<Ra__Node*> args;
};

class Ra__Node__Case_Expr: public Ra__Node {
    public:
        Ra__Node__Case_Expr(){
            node_case = Ra__Node__NodeCase::RA__NODE__CASE_EXPR;
            n_children = 0;
            else_default = nullptr;
        }
    std::vector<Ra__Node__Case_When*> args;
    Ra__Node* else_default;
};

class Ra__Node__Case_When: public Ra__Node {
    public:
        Ra__Node__Case_When(){
            node_case = Ra__Node__NodeCase::RA__NODE__CASE_WHEN;
            n_children = 0;
        }
    Ra__Node* when; // predicate
    Ra__Node* then; // expression
};

class Ra__Node__Dummy: public Ra__Node {
    public:
        Ra__Node__Dummy(){
            node_case = Ra__Node__NodeCase::RA__NODE__DUMMY;
            n_children = 0;
        }
};
#endif