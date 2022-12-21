#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <map>
#include <cassert>
#include <algorithm>

#include "deparse_ra_to_sql.h"

RAtoSQL::RAtoSQL(std::shared_ptr<RaTree> _raTree)
:raTree(_raTree){
};

std::string RAtoSQL::deparse_expression(std::shared_ptr<Ra__Node> arg){
    std::string result = "";

    switch(arg->node_case){
        case RA__NODE__CONST: {
            auto constant = std::static_pointer_cast<Ra__Node__Constant>(arg);
            result += constant->to_string();
            break;
        }
        case RA__NODE__ATTRIBUTE: {
            auto attr = std::static_pointer_cast<Ra__Node__Attribute>(arg);
            result += attr->to_string();
            break;
        }
        case RA__NODE__FUNC_CALL: {
            auto func_call = std::static_pointer_cast<Ra__Node__Func_Call>(arg);
            if(func_call->func_name=="substring"){
                switch(func_call->args.size()){
                    case 1: result += func_call->func_name + "(" + deparse_expression(func_call->args[0]) + ")"; break;
                    case 2: result += func_call->func_name + "(" + deparse_expression(func_call->args[0]) + " from " + deparse_expression(func_call->args[1]) + ")"; break;
                    case 3: result += func_call->func_name + "(" + deparse_expression(func_call->args[0]) + " from " + deparse_expression(func_call->args[1]) + " for " + deparse_expression(func_call->args[2]) + ")"; break;
                    default: std::cout << "too many args in substring func call" << std::endl;
                };
            }
            else if(func_call->func_name=="extract"){
                assert(func_call->args.size()==2);
                std::string date_part = deparse_expression(func_call->args[0]);
                date_part.erase(std::remove(date_part.begin(), date_part.end(), '\''), date_part.end());
                result += func_call->func_name + "(" + date_part + " from " + deparse_expression(func_call->args[1]) + ")"; break;
            }
            else if(func_call->is_aggregating && func_call->agg_distinct){
                result += func_call->func_name + "(distinct " + deparse_expressions(func_call->args) + ")";
            }
            else{
                result += func_call->func_name + "(" + deparse_expressions(func_call->args) + ")";
            }
            break;
        }
        case RA__NODE__TYPE_CAST: {
            auto type_cast = std::static_pointer_cast<Ra__Node__Type_Cast>(arg);
            if(type_cast->typ_mod.length()>0)
                result += type_cast->type + " " + deparse_expression(type_cast->expression) + " " + type_cast->typ_mod;
            else
                result += type_cast->type + " " + deparse_expression(type_cast->expression);
            break;
        }
        case RA__NODE__SELECT_EXPRESSION:{
            auto sel_expr = std::static_pointer_cast<Ra__Node__Select_Expression>(arg);
            result += deparse_expression(sel_expr->expression);
            if(sel_expr->rename.size()>0){
                result += " as " + sel_expr->rename;
            }
            break;
        }
        case RA__NODE__EXPRESSION:{
            auto expr = std::static_pointer_cast<Ra__Node__Expression>(arg);
            result += "(";
            if(expr->l_arg!=nullptr){
                result += deparse_expression(expr->l_arg);
            }
            result += expr->operator_;
            if(expr->r_arg!=nullptr){
                result += deparse_expression(expr->r_arg);
            }
            result += ")";
            break;
        }
        case RA__NODE__LIST:{
            auto list = std::static_pointer_cast<Ra__Node__List>(arg);
            for(size_t i=0;i<list->args.size();i++){
                result += i<list->args.size()-1 ? deparse_expression(list->args[i])+" and " : deparse_expression(list->args[i]);
            }
            break;
        }
        case RA__NODE__CASE_EXPR:{
            auto case_expr = std::static_pointer_cast<Ra__Node__Case_Expr>(arg);
            result += "case";
            for(auto& arg: case_expr->args){
                result += " when " + deparse_predicate(arg->when) + " then " + deparse_expression(arg->then);
            }
            if(case_expr->else_default!=nullptr){
                result += " else " + deparse_expression(case_expr->else_default);
            }
            result += " end";
            break;
        }
        case RA__NODE__DUMMY: break; //e.g. (dummy)-name
        default: std::cout << "error deparse select" << std::endl;
    }

    return result;
}

std::string RAtoSQL::deparse_order_by_expressions(std::vector<std::shared_ptr<Ra__Node>>& expressions, std::vector<Ra__Order_By__SortDirection>& directions){
    std::string result = "";
    assert(expressions.size()>0);
    assert(expressions.size()==directions.size());
    size_t n = expressions.size();

    for(size_t i=0; i<n; i++){
        std::string direction;
        switch(directions[i]){
            case RA__ORDER_BY__DEFAULT: direction = ""; break;
            case RA__ORDER_BY__ASC: direction = " asc"; break;
            case RA__ORDER_BY__DESC: direction = " desc"; break;
            default: std::cout << "deparse order by order error" << std::endl; direction = "";
        }
        result += deparse_expression(expressions[i]) + direction + ", ";
    }
    result.pop_back();
    result.pop_back();
    return result;
}

std::string RAtoSQL::deparse_expressions(std::vector<std::shared_ptr<Ra__Node>>& expressions){
    std::string result = "";

    assert(expressions.size()>0);
    for(auto expression: expressions){
        result += deparse_expression(expression) + ", ";
    }
    result.pop_back();
    result.pop_back();
    return result;
}

std::string RAtoSQL::deparse_predicate(std::shared_ptr<Ra__Node> node){
    switch(node->node_case){
        case RA__NODE__BOOL_PREDICATE: {
            std::string result = "";
            auto p = std::static_pointer_cast<Ra__Node__Bool_Predicate>(node);
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
                        switch(arg->node_case){
                            case RA__NODE__BOOL_PREDICATE:{
                                result += op + "(" + deparse_predicate(arg) + ")";
                                break;
                            }
                            case RA__NODE__WHERE_SUBQUERY_MARKER:{
                                if(std::static_pointer_cast<Ra__Node__Where_Subquery_Marker>(arg)->type==RA__JOIN__ANTI_IN_LEFT 
                                || std::static_pointer_cast<Ra__Node__Where_Subquery_Marker>(arg)->type==RA__JOIN__ANTI_IN_LEFT_DEPENDENT){
                                    result += deparse_predicate(arg);
                                }
                                else{
                                    result += op + deparse_predicate(arg);
                                }
                                break;
                            }
                            case RA__NODE__NULL_TEST:{
                                result += deparse_predicate(arg);
                                break;
                            }
                            default: result += op + deparse_predicate(arg);
                        }
                    }
                    break;
                }
            }
            return result;
        }
        case RA__NODE__PREDICATE: {
            auto p = std::static_pointer_cast<Ra__Node__Predicate>(node);
            return deparse_predicate(p->left) + p->binaryOperator + deparse_predicate(p->right);
        }
        case RA__NODE__NULL_TEST:{
            std::string result;
            auto null_test = std::static_pointer_cast<Ra__Node__Null_Test>(node);
            result += deparse_expression(null_test->arg);
            switch(null_test->type){
                case RA__NULL_TEST__IS_NULL:{
                    result += " is null";
                    break;
                }
                case RA__NULL_TEST__IS_NOT_NULL:{
                    result += " is not null";
                    break;
                }
            }
            return result;
        }
        case RA__NODE__IN_LIST: {
            auto list = std::static_pointer_cast<Ra__Node__In_List>(node);
            std::string str = "(";
            for(auto& arg: list->args){
                str += deparse_expression(arg) + ",";
            }
            str.pop_back();
            str += ")";

            return str;
        }
        case RA__NODE__WHERE_SUBQUERY_MARKER:{
            auto marker = std::static_pointer_cast<Ra__Node__Where_Subquery_Marker>(node);
            // find join
            std::shared_ptr<Ra__Node> it = raTree->root;
            assert(find_marker_subquery(it, marker));
            auto join = std::static_pointer_cast<Ra__Node__Join>(it);
            std::shared_ptr<Ra__Node> subquery = nullptr;
            subquery = join->childNodes[1];
            switch(join->type){
                case RA__JOIN__SEMI_LEFT: 
                case RA__JOIN__SEMI_LEFT_DEPENDENT:
                case RA__JOIN__ANTI_LEFT: 
                case RA__JOIN__ANTI_LEFT_DEPENDENT: {
                    // exists
                    return "exists (" + deparse_projection(subquery)+")";
                }
                case RA__JOIN__IN_LEFT:
                case RA__JOIN__IN_LEFT_DEPENDENT:{
                    // in
                    auto p = std::static_pointer_cast<Ra__Node__Predicate>(join->predicate);
                    return deparse_expression(p->left) + " in (" + deparse_projection(subquery)+")";
                }
                case RA__JOIN__ANTI_IN_LEFT:
                case RA__JOIN__ANTI_IN_LEFT_DEPENDENT:{
                    // not in
                    auto p = std::static_pointer_cast<Ra__Node__Predicate>(join->predicate);
                    return deparse_expression(p->left) + " not in (" + deparse_projection(subquery)+")";
                }
                default: return "("+deparse_projection(subquery)+")";
            }
            // deparse subquery
            
        }
        default: {
            auto expression = std::static_pointer_cast<Ra__Node__Expression>(node);
            return deparse_expression(expression);
        }
    }
}

bool RAtoSQL::find_marker_subquery(std::shared_ptr<Ra__Node>& it, std::shared_ptr<Ra__Node__Where_Subquery_Marker> marker){
    if(it->node_case==RA__NODE__JOIN){
        auto join = std::static_pointer_cast<Ra__Node__Join>(it);
        if(join->right_where_subquery_marker==marker){
            return true;
        }
    }
    
    if(it->n_children == 0){
        return false;
    }

    bool found = false;
    std::vector<std::shared_ptr<Ra__Node>> childNodes = it->childNodes;
    for(auto child: childNodes){
        if(!found){
            it = child;
            found = find_marker_subquery(it, marker);
        }
    }
    return found;
}

std::string RAtoSQL::deparse_selection(std::shared_ptr<Ra__Node> node){
    return deparse_predicate(std::static_pointer_cast<Ra__Node__Selection>(node)->predicate);
}

std::string RAtoSQL::deparse_relation(std::shared_ptr<Ra__Node> node){
    auto relation = std::static_pointer_cast<Ra__Node__Relation>(node);
    return relation->alias.length()>0 ? relation->name + " " + relation->alias : relation->name;
}

void RAtoSQL::deparse_ra_node(std::shared_ptr<Ra__Node> node, size_t layer, 
    std::string& select, 
    std::string& where, 
    std::string& from,
    std::string& group_by,
    std::string& having,
    std::string& order_by){

    assert(node->is_full());
    switch(node->node_case){
        case RA__NODE__ROOT: {
            deparse_ra_node(node->childNodes[0], layer, select, where, from, group_by, having, order_by);
            break;
        }
        case RA__NODE__SELECTION: {
            if(where.length()>0 && std::static_pointer_cast<Ra__Node__Selection>(node)->predicate->node_case==RA__NODE__BOOL_PREDICATE){
                where += " and ";
                where += "(" + deparse_selection(node) + ")";
            }
            else if(where.length()>0){
                where += " and ";
                where += deparse_selection(node);
            }
            else{
                where += deparse_selection(node);
            }
            deparse_ra_node(node->childNodes[0], layer, select, where, from, group_by, having, order_by);
            break;
        }
        case RA__NODE__PROJECTION: {
            auto pr = std::static_pointer_cast<Ra__Node__Projection>(node);
            if(layer==0){
                select = "select ";
                if(pr->distinct){
                    select += "distinct ";
                }
                select += deparse_expressions(pr->args) + ", ";
                layer++;
                deparse_ra_node(node->childNodes[0], layer, select, where, from, group_by, having, order_by);
            }
            else{
                if(pr->subquery_columns.size()>0){
                    std::string subquery_columns = "";
                    for(auto& col_rename: pr->subquery_columns){
                        subquery_columns += col_rename + ",";
                    }
                    subquery_columns.pop_back();
                    from += "(" + deparse_projection(node) + ") as " + pr->subquery_alias + "(" + subquery_columns + ")";
                }
                else{
                    from += "(" + deparse_projection(node) + ") as " + pr->subquery_alias; 
                }
            }
            break;
        }
        case RA__NODE__GROUP_BY: {
            auto gb = std::static_pointer_cast<Ra__Node__Group_By>(node);
            if(!gb->implicit){
                group_by += deparse_expressions(gb->args);
            }
            deparse_ra_node(gb->childNodes[0], layer, select, where, from, group_by, having, order_by);
            break;
        }
        case RA__NODE__ORDER_BY: {
            auto ob = std::static_pointer_cast<Ra__Node__Order_By>(node);
            order_by += deparse_order_by_expressions(ob->args, ob->directions);
            deparse_ra_node(ob->childNodes[0], layer, select, where, from, group_by, having, order_by);
            break;
        }
        case RA__NODE__HAVING: {
            auto ha = std::static_pointer_cast<Ra__Node__Having>(node);
            having += deparse_predicate(ha->predicate);
            deparse_ra_node(ha->childNodes[0], layer, select, where, from, group_by, having, order_by);
            break;
        }
        case RA__NODE__RELATION: {
            from += deparse_relation(node);
            break;
        }
        case RA__NODE__JOIN: {
            auto join = std::static_pointer_cast<Ra__Node__Join>(node);
            // switch case join type
            assert(join->is_full());
            if(std::static_pointer_cast<Ra__Node__Join>(node)->right_where_subquery_marker->marker>0){
                deparse_ra_node(join->childNodes[0], layer, select, where, from, group_by, having, order_by);
                break;
            }
            switch(join->type){
                case RA__JOIN__CROSS_PRODUCT:{
                    deparse_ra_node(join->childNodes[0], layer, select, where, from, group_by, having, order_by);
                    from += ", ";
                    deparse_ra_node(join->childNodes[1], layer, select, where, from, group_by, having, order_by);
                    break;
                }
                case RA__JOIN__INNER: 
                case RA__JOIN__LEFT: 
                case RA__JOIN__DEPENDENT_INNER_LEFT:  {
                    // from += "(";
                    deparse_ra_node(join->childNodes[0],layer, select, where, from, group_by, having, order_by);
                    from += join->join_name();
                    deparse_ra_node(join->childNodes[1],layer, select, where, from, group_by, having, order_by);
                    if(join->predicate!=nullptr){
                        from +=  " on " + deparse_predicate(join->predicate);
                    }
                    // from += ")";
                    if(join->alias.length()>0){
                        from += " as " + join->alias; 
                    } 
                    if(join->columns.size()>0){
                        std::string columns = "(";
                        for(auto& col: join->columns){
                            columns += col + ",";
                        }
                        columns.pop_back();
                        columns += ")";
                        from += columns;
                    };
                    break;
                }
                // exists, not exists
                case RA__JOIN__SEMI_LEFT: 
                case RA__JOIN__SEMI_LEFT_DEPENDENT:
                case RA__JOIN__ANTI_LEFT: 
                case RA__JOIN__ANTI_LEFT_DEPENDENT: {
                    std::cout << "Right subquery should have marker" << std::endl;
                    break;
                }
                default: std::cout << "Join type not supported in decorrelation" << std::endl;
            }
            break;
        }
        case RA__NODE__VALUES:{
            auto values = std::static_pointer_cast<Ra__Node__Values>(node);
            from += "(values";
            for(auto& v: values->values){
                from += "(" + deparse_expression(v) + "),";
            }
            from.pop_back();
            from += ") as " + values->alias + "(" + values->column + ")";
            break;
        }
        default: {
            // nothing to do
            break;
        }
    }
}

std::string RAtoSQL::deparse_projection(std::shared_ptr<Ra__Node> node){

    std::string select = "";
    std::string from = "";
    std::string where = "";
    std::string group_by = "";
    std::string having = "";
    std::string order_by = "";

    deparse_ra_node(node, 0, select, where, from, group_by, having, order_by);

    if(select.length()==0){
        select = "*";
    }
    else{
        select.pop_back();
        select.pop_back();
    }
    std::string sql = select + "\n";
    
    if(from.length()>0){
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

std::string RAtoSQL::deparse_ctes(std::vector<std::shared_ptr<Ra__Node>> ctes){
    std::string sql = "";
    if(ctes.size()>0){
        sql += "with ";
        for(auto& cte: ctes){
            auto cte_pr = std::static_pointer_cast<Ra__Node__Projection>(cte);
            std::string cte_cols = "";
            if(cte_pr->subquery_columns.size()>0){
                cte_cols += "(";
                for(auto& col: cte_pr->subquery_columns){
                    cte_cols += col + ",";
                }
                cte_cols.pop_back();
                cte_cols += ")";
            }
            sql += cte_pr->subquery_alias + cte_cols + " as (\n";
            sql += deparse_projection(cte);
            sql += "),";
        }
        sql.pop_back();
        sql += "\n";
    }
    return sql;
}

std::string RAtoSQL::deparse(){
    std::string cte_str = deparse_ctes(raTree->ctes);
    std::string select_str = deparse_projection(raTree->root);
    return cte_str + select_str;
}