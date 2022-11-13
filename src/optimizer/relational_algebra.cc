#include "relational_algebra.h"

Ra__Node::~Ra__Node(){
    for(auto child: childNodes){
        delete child;
    }
}

std::string Ra__Node::to_string() {
    return "";
}

bool Ra__Node::is_full(){
    return n_children == childNodes.size();
}

Ra__Node__Join::Ra__Node__Join(Ra__Join__JoinType _type, uint64_t l_marker, uint64_t r_marker)
: type(_type),left_where_subquery_marker(new Ra__Node__Where_Subquery_Marker(l_marker,_type)),right_where_subquery_marker(new Ra__Node__Where_Subquery_Marker(r_marker,_type))
{
    node_case = Ra__Node__NodeCase::RA__NODE__JOIN;
    n_children = 2;
    predicate = nullptr;
}

Ra__Node__Join::~Ra__Node__Join(){
    delete predicate;
}

std::string Ra__Node__Join::to_string(){
    assert(this->is_full());
    std::string op;
    switch(type){
        case RA__JOIN__CROSS_PRODUCT: op = "X"; break;
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
        case RA__JOIN__ANTI_LEFT_DEPENDENT: op = "ALDJ"; break;
        case RA__JOIN__IN_LEFT: op = "ILJ"; break;
        case RA__JOIN__IN_LEFT_DEPENDENT: op = "ILDJ"; break;
        case RA__JOIN__ANTI_IN_LEFT: op = "AILJ"; break;
        case RA__JOIN__ANTI_IN_LEFT_DEPENDENT: op = "AILDJ"; break;
        default: op = "join op not supported";
    }
    return "(" + childNodes[0]->to_string() + ")"+op+"(" + childNodes[1]->to_string() + ")";
}

std::string Ra__Node__Join::join_name(){
    switch(type){
        case RA__JOIN__DEPENDENT_INNER_RIGHT:
        case RA__JOIN__DEPENDENT_INNER_LEFT:
        case RA__JOIN__INNER: return " join ";
        case RA__JOIN__LEFT: return " left join ";
        case RA__JOIN__RIGHT: return " right join ";
        default: return " join op not supported ";
    }
}

Ra__Node__Projection::~Ra__Node__Projection(){
    for(auto arg: args){
        delete arg;
    }
}

Ra__Node__Projection::Ra__Node__Projection(){
    node_case = Ra__Node__NodeCase::RA__NODE__PROJECTION;
    n_children = 1;
    distinct = false;
}

std::string Ra__Node__Projection::to_string(){
    assert(childNodes.size()==1);
    return "\u03A0(" + childNodes[0]->to_string() + ")";
}

Ra__Node__Selection::Ra__Node__Selection(){
    node_case = Ra__Node__NodeCase::RA__NODE__SELECTION;
    n_children = 1;
    predicate = nullptr;
}

Ra__Node__Selection::~Ra__Node__Selection(){
    delete predicate;
}

std::string Ra__Node__Selection::to_string(){
    assert(childNodes.size()==1);
    return "\u03C3(" + childNodes[0]->to_string() + ")";
}

Ra__Node__Relation::Ra__Node__Relation(std::string _name, std::string _alias)
:name(_name),alias(_alias){
    node_case = Ra__Node__NodeCase::RA__NODE__RELATION;
    n_children = 0;
}

Ra__Node__Relation::~Ra__Node__Relation(){
    for(auto attr: attributes){
        delete attr;
    }
}

std::string Ra__Node__Relation::to_string(){
    assert(childNodes.size()==0);
    return alias.length()==0 ? name : name + " " + alias;
}

Ra__Node__Order_By::Ra__Node__Order_By(){
    node_case = Ra__Node__NodeCase::RA__NODE__ORDER_BY;
    n_children = 1;
}

Ra__Node__Order_By::~Ra__Node__Order_By(){
    for(auto arg: args){
        delete arg;
    }
}

std::string Ra__Node__Order_By::to_string(){
    assert(childNodes.size()==1);
    return "OB(" + childNodes[0]->to_string() + ")";
}

Ra__Node__Group_By::Ra__Node__Group_By(bool _implicit)
:implicit(_implicit){
    node_case = Ra__Node__NodeCase::RA__NODE__GROUP_BY;
    n_children = 1;
}

Ra__Node__Group_By::~Ra__Node__Group_By(){
    for(auto arg: args){
        delete arg;
    }
}

std::string Ra__Node__Group_By::to_string(){
    assert(childNodes.size()==1);
    return "\u0393(" + childNodes[0]->to_string() + ")";
}

Ra__Node__Having::Ra__Node__Having(){
    node_case = Ra__Node__NodeCase::RA__NODE__HAVING;
    n_children = 1;
    predicate = nullptr;
}

Ra__Node__Having::~Ra__Node__Having(){
    delete predicate;
}

std::string Ra__Node__Having::to_string(){
    assert(childNodes.size()==1);
    return "H(" + childNodes[0]->to_string() + ")";
}

Ra__Node__Bool_Predicate::Ra__Node__Bool_Predicate(){
    node_case = Ra__Node__NodeCase::RA__NODE__BOOL_PREDICATE;
    n_children = 0;
}

Ra__Node__Bool_Predicate::~Ra__Node__Bool_Predicate(){
    for(auto arg: args){
        delete arg;
    }
}


Ra__Node__Predicate::Ra__Node__Predicate() {
    node_case = Ra__Node__NodeCase::RA__NODE__PREDICATE;
    n_children = 0;
    left = nullptr;
    right = nullptr;
}

Ra__Node__Predicate::~Ra__Node__Predicate(){
    delete left;
    delete right;
}

Ra__Node__Select_Expression::Ra__Node__Select_Expression(){
    node_case = Ra__Node__NodeCase::RA__NODE__SELECT_EXPRESSION;
    n_children = 0;
    expression = new Ra__Node__Expression();
}

Ra__Node__Select_Expression::~Ra__Node__Select_Expression(){
    delete expression;
}

Ra__Node__Expression::Ra__Node__Expression(){
    node_case = Ra__Node__NodeCase::RA__NODE__EXPRESSION;
    n_children = 0;
    r_arg = nullptr;
    l_arg = nullptr;
}

Ra__Node__Expression::~Ra__Node__Expression(){
    delete r_arg;
    delete l_arg;
}

void Ra__Node__Expression::add_arg(Ra__Node* arg){
    if(r_arg==nullptr) r_arg=arg;
    else if(l_arg==nullptr) l_arg=arg;
    else std::cout << "error in expression add arg, already full" << std::endl;
}

Ra__Node__Type_Cast::Ra__Node__Type_Cast(std::string _type, std::string _typ_mod, Ra__Node* _expression)
:type(_type),typ_mod(_typ_mod),expression(_expression){
    node_case = Ra__Node__NodeCase::RA__NODE__TYPE_CAST;
    n_children = 0;
}

Ra__Node__Type_Cast::~Ra__Node__Type_Cast(){
    delete expression;
}

std::string Ra__Node__Type_Cast::to_string(){
    return "";
}

Ra__Node__Attribute::Ra__Node__Attribute(std::string _name, std::string _alias)
:name(_name), alias(_alias){
    node_case = Ra__Node__NodeCase::RA__NODE__ATTRIBUTE;
    n_children = 0;
}

std::string Ra__Node__Attribute::to_string(){
    return alias.length()>0 ? alias+"."+name : name;
}

Ra__Node__Constant::Ra__Node__Constant(std::string _data, Ra__Const_DataType__DataType _dataType)
:data(_data), dataType(_dataType){
    node_case = Ra__Node__NodeCase::RA__NODE__CONST;
    n_children = 0;
}

std::string Ra__Node__Constant::to_string(){
    switch(dataType){
        case RA__CONST_DATATYPE__STRING:
            return "'" + data + "'";
        default:
            return data;
    }
}

Ra__Node__Func_Call::Ra__Node__Func_Call(std::string _func_name)
:func_name(_func_name){
    node_case = Ra__Node__NodeCase::RA__NODE__FUNC_CALL;
    n_children = 0;
    is_aggregating = false;
    agg_distinct = false;
}

Ra__Node__Func_Call::~Ra__Node__Func_Call(){
    for(auto arg: args){
        delete arg;
    }
}

Ra__Node__List::Ra__Node__List(){
    node_case = Ra__Node__NodeCase::RA__NODE__LIST;
    n_children = 0;
}

Ra__Node__List::~Ra__Node__List(){
    for(auto arg: args){
        delete arg;
    }
}

Ra__Node__In_List::Ra__Node__In_List(){
    node_case = Ra__Node__NodeCase::RA__NODE__IN_LIST;
    n_children = 0;
}

Ra__Node__In_List::~Ra__Node__In_List(){
    for(auto arg: args){
        delete arg;
    }
}

Ra__Node__Case_Expr::Ra__Node__Case_Expr(){
    node_case = Ra__Node__NodeCase::RA__NODE__CASE_EXPR;
    n_children = 0;
    else_default = nullptr;
}

Ra__Node__Case_Expr::~Ra__Node__Case_Expr(){
    for(auto arg: args){
        delete arg;
    }
}

Ra__Node__Case_When::Ra__Node__Case_When(Ra__Node* _when, Ra__Node* _then)
:when(_when), then(_then){
    node_case = Ra__Node__NodeCase::RA__NODE__CASE_WHEN;
    n_children = 0;
}

Ra__Node__Case_When::~Ra__Node__Case_When(){
    delete when;
    delete then;
}

Ra__Node__Values::Ra__Node__Values(){
    node_case = Ra__Node__NodeCase::RA__NODE__VALUES;
    n_children = 0;
}

Ra__Node__Values::~Ra__Node__Values(){
    for(auto value: values){
        delete value;
    }
}

std::string Ra__Node__Values::to_string(){
    return alias;
}

Ra__Node__Null_Test::Ra__Node__Null_Test(){
    node_case = Ra__Node__NodeCase::RA__NODE__NULL_TEST;
    n_children = 0;
    arg = nullptr;
}

Ra__Node__Null_Test::~Ra__Node__Null_Test(){
    delete arg;
}

Ra__Node__Where_Subquery_Marker::Ra__Node__Where_Subquery_Marker(uint64_t _marker, Ra__Join__JoinType _type)
:marker(_marker), type(_type){
    node_case = Ra__Node__NodeCase::RA__NODE__WHERE_SUBQUERY_MARKER;
}

std::string Ra__Node__Where_Subquery_Marker::to_string()
{
    return std::to_string(marker);
}

Ra__Node__Dummy::Ra__Node__Dummy(){
    node_case = Ra__Node__NodeCase::RA__NODE__DUMMY;
    n_children = 0;
}