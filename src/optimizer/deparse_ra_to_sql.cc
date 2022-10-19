#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <map>
#include <cassert>

#include "deparse_ra_to_sql.h"

std::string deparse_expression(Ra__Node__Expression* expression){
    std::string result = "";
    assert(expression->args.size()>0);

    size_t i = 0;
    do{
        if(i>0){
            result += expression->operators[i-1];
        }
        switch(expression->args[i]->node_case){
            case RA__NODE__CONST: {
                Ra__Node__Constant* constant = static_cast<Ra__Node__Constant*>(expression->args[i]);
                result += constant->to_string();
                break;
            }
            case RA__NODE__ATTRIBUTE: {
                Ra__Node__Attribute* attr = static_cast<Ra__Node__Attribute*>(expression->args[i]);
                result += attr->to_string();
                break;
            }
            case RA__NODE__FUNC_CALL: {
                Ra__Node__Func_Call* func_call = static_cast<Ra__Node__Func_Call*>(expression->args[i]);
                result += func_call->func_name + "(" + deparse_expression(func_call->expression) + ")";
                break;
            }
            default: std::cout << "error deparse select" << std::endl;
        }
        i++;
    }while(i<expression->args.size());
    
    if(expression->rename.size()>0){
        result += " as " + expression->rename;
    }

    return result;
}

std::string generate_select(Ra__Node* node){
    std::string result = "";
    auto projection = static_cast<Ra__Node__Projection*>(node);

    for(auto expression: projection->expressions){
        result += deparse_expression(expression) + ", ";
    }
    result.pop_back();
    result.pop_back();
    return result;
}

std::string deparse_predicate(Ra__Node* node){
    switch(node->node_case){
        case RA__NODE__BOOL_PREDICATE: {
            std::string result = "";
            Ra__Node__Bool_Predicate* p = static_cast<Ra__Node__Bool_Predicate*>(node);
            std::string op;
            switch(p->bool_operator){
                case RA__BOOL_OPERATOR__AND: {
                    op = " and ";   
                    for(auto arg: p->args){
                        result += deparse_predicate(arg) + op;
                    }
                    for(size_t i=0; i<op.length();i++){
                        result.pop_back();
                    }
                    break;
                }
                case RA__BOOL_OPERATOR__OR: {
                    op = " or ";    
                    for(auto arg: p->args){
                        result += deparse_predicate(arg) + op;
                    }
                    for(size_t i=0; i<op.length();i++){
                        result.pop_back();
                    }
                    break;
                }
                case RA__BOOL_OPERATOR__NOT: {
                    op = "not ";
                    for(auto arg: p->args){
                        result += op + deparse_predicate(arg);
                    }
                    break;
                }
            }
            return result;
        }
        case RA__NODE__PREDICATE: {
            Ra__Node__Predicate* p = static_cast<Ra__Node__Predicate*>(node);
            return deparse_predicate(p->left) + p->binaryOperator + deparse_predicate(p->right);
        }
        case RA__NODE__EXPRESSION: {
            Ra__Node__Expression* expression = static_cast<Ra__Node__Expression*>(node);
            return deparse_expression(expression);
        }
        default: {
            std::cout << "error deparse predicate" << std::endl;
            return "deparse_error";
        }
    }
}

std::string generate_where(Ra__Node* node){
    Ra__Node__Selection* sel = static_cast<Ra__Node__Selection*>(node);
    return deparse_predicate(sel->predicate);
}

std::string generate_from(Ra__Node* node){
    Ra__Node__Relation* relation = static_cast<Ra__Node__Relation*>(node);
    return relation->alias.length()>0 ? relation->name + " " + relation->alias : relation->name;
}

void ra_tree_dfs(Ra__Node* node, size_t layer, std::string& select, std::string& where, std::string& from){

    assert(node->is_full());
    switch(node->node_case){
        case RA__NODE__ROOT: {
            ra_tree_dfs(node->childNodes[0], layer, select, where, from);
            break;
        }
        case RA__NODE__SELECTION: {
            where += generate_where(node);
            ra_tree_dfs(node->childNodes[0], layer, select, where, from);
            break;
        }
        case RA__NODE__PROJECTION: {
            if(layer==0){
                select = "select " + generate_select(node) + ", ";
                layer++;
                ra_tree_dfs(node->childNodes[0], layer, select, where, from);
            }
            else{
                Ra__Node__Projection* pr = static_cast<Ra__Node__Projection*>(node);
                from += "(" + deparse_ra(node) + ") as " + pr->subquery_alias + ", "; 
            }
            break;
        }
        case RA__NODE__CROSS_PRODUCT: {
            ra_tree_dfs(node->childNodes[0], layer, select, where, from);
            ra_tree_dfs(node->childNodes[1], layer, select, where, from);
            break;
        }
        case RA__NODE__RELATION: {
            from += generate_from(node) + ", ";
            break;
        }
        default: {
            // nothing to do
            break;
        }
    }
    // for(auto child: node->childNodes){
    //     ra_tree_dfs(child, layer, select, where, from);
    // }
}

std::string deparse_ra(Ra__Node* ra_root){

    std::string select = "";
    std::string from = "";
    std::string where = "";

    ra_tree_dfs(ra_root, 0, select, where, from);

    if(select.length()==0){
        select = "*";
    }
    else{
        select.pop_back();
        select.pop_back();
    }
    std::string sql = select + "\n";
    
    if(from.length()>0){
        from.pop_back(); // remove ", "
        from.pop_back();
        sql += "from " + from + "\n";
    }
    
    if(where.length()>0){
        sql += "where " + where + "\n";
    }

    return sql;
}
