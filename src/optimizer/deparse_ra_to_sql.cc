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
            case RA__NODE__DUMMY: break; //e.g. (dummy)-name
            default: std::cout << "error deparse select" << std::endl;
        }
        i++;
    }while(i<expression->args.size());
    
    if(expression->rename.size()>0){
        result += " as " + expression->rename;
    }

    return result;
}

std::string deparse_order_by_expressions(std::vector<Ra__Node__Expression*>& expressions, std::vector<Ra__Order_By_Direction>& directions){
    std::string result = "";
    assert(expressions.size()==directions.size());
    size_t n = expressions.size();

    for(size_t i=0; i<n; i++){
        std::string direction;
        switch(directions[i]){
            case RA__ORDER_BY_ASC: direction = "asc"; break;
            case RA__ORDER_BY_DESC: direction = "desc"; break;
            default: std::cout << "deparse order by order error" << std::endl; direction = "";
        }
        result += deparse_expression(expressions[i]) + " " + direction + ", ";
    }
    result.pop_back();
    result.pop_back();
    return result;
}

std::string deparse_expressions(std::vector<Ra__Node__Expression*>& expressions){
    std::string result = "";

    for(auto expression: expressions){
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
                        if(arg->node_case==RA__NODE__BOOL_PREDICATE){
                            result += "(" + deparse_predicate(arg) + ")" + op;
                        }
                        else{
                            result += deparse_predicate(arg) + op;
                        }
                    }
                    for(size_t i=0; i<op.length();i++){
                        result.pop_back();
                    }
                    break;
                }
                case RA__BOOL_OPERATOR__OR: {
                    op = " or ";    
                    for(auto arg: p->args){
                        if(arg->node_case==RA__NODE__BOOL_PREDICATE){
                            result += "(" + deparse_predicate(arg) + ")" + op;
                        }
                        else{
                            result += deparse_predicate(arg) + op;
                        }
                    }
                    for(size_t i=0; i<op.length();i++){
                        result.pop_back();
                    }
                    break;
                }
                case RA__BOOL_OPERATOR__NOT: {
                    op = "not ";
                    for(auto arg: p->args){
                        if(arg->node_case==RA__NODE__BOOL_PREDICATE){
                            result += op + "(" + deparse_predicate(arg) + ")";
                        }
                        else{
                            result += op + deparse_predicate(arg);
                        }
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

void ra_tree_dfs(Ra__Node* node, size_t layer, 
    std::string& select, 
    std::string& where, 
    std::string& from,
    std::string& group_by,
    std::string& having,
    std::string& order_by){

    assert(node->is_full());
    switch(node->node_case){
        case RA__NODE__ROOT: {
            ra_tree_dfs(node->childNodes[0], layer, select, where, from, group_by, having, order_by);
            break;
        }
        case RA__NODE__SELECTION: {
            where += generate_where(node);
            ra_tree_dfs(node->childNodes[0], layer, select, where, from, group_by, having, order_by);
            break;
        }
        case RA__NODE__PROJECTION: {
            Ra__Node__Projection* pr = static_cast<Ra__Node__Projection*>(node);
            if(layer==0){
                select = "select " + deparse_expressions(pr->args) + ", ";
                layer++;
                ra_tree_dfs(node->childNodes[0], layer, select, where, from, group_by, having, order_by);
            }
            else{
                from += "(" + deparse_ra(node) + ") as " + pr->subquery_alias + ", "; 
            }
            break;
        }
        case RA__NODE__GROUP_BY: {
            Ra__Node__Group_By* gb = static_cast<Ra__Node__Group_By*>(node);
            group_by += deparse_expressions(gb->args);
            ra_tree_dfs(gb->childNodes[0], layer, select, where, from, group_by, having, order_by);
            break;
        }
        case RA__NODE__ORDER_BY: {
            Ra__Node__Order_By* ob = static_cast<Ra__Node__Order_By*>(node);
            order_by += deparse_order_by_expressions(ob->args, ob->directions);
            ra_tree_dfs(ob->childNodes[0], layer, select, where, from, group_by, having, order_by);
            break;
        }
        case RA__NODE__HAVING: {
            Ra__Node__Having* ha = static_cast<Ra__Node__Having*>(node);
            having += deparse_predicate(ha->predicate);
            ra_tree_dfs(ha->childNodes[0], layer, select, where, from, group_by, having, order_by);
            break;
        }
        case RA__NODE__CROSS_PRODUCT: {
            ra_tree_dfs(node->childNodes[0], layer, select, where, from, group_by, having, order_by);
            ra_tree_dfs(node->childNodes[1], layer, select, where, from, group_by, having, order_by);
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
}

std::string deparse_ra(Ra__Node* ra_root){

    std::string select = "";
    std::string from = "";
    std::string where = "";
    std::string group_by = "";
    std::string having = "";
    std::string order_by = "";

    ra_tree_dfs(ra_root, 0, select, where, from, group_by, having, order_by);

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

    if(group_by.length()>0){
        sql += "group by " + group_by + "\n";
    }

    if(having.length()>0){
        sql += "having " + having + "\n";
    }

    if(order_by.length()>0){
        sql += "order by " + order_by + "\n";
    }

    return sql;
}
