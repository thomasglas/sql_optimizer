#include "ra_tree.h"
#include <tuple>

RaTree::RaTree(Ra__Node* _root, std::vector<Ra__Node*> _ctes)
:root(_root), ctes(_ctes){
}

RaTree::~RaTree(){
    delete root;
    for(auto cte: ctes){
        delete cte;
    }
};

void RaTree::optimize(){
    /**
     * select s.studnr
     * from students s
     * where exists (select p.persnr
     *              from professors p
     *              where p.persnr=s.studnr)
     */
    // ->
    /**
     * select s.studnr
     * from students s
     * where s.studnr in (select p.persnr
     *                  from professors p) 
     */
    decorrelate_simple_exists();
};

// 1. find all "exists" markers
// 2. find all "exists" joins belonging to markers
// 3. for each "exists", check if subquery simply correlated, decorrelate:
    // remove correlating predicate from subquery selection
    // set subquery select target to inner predicate attribute
    // change "exists" join to "in" join
        // set predicate attr to "in" probe (join->predicate->left)
    // change selection marker type from "exists" to "in"
void RaTree::decorrelate_simple_exists(){
    // find marker in selection
    std::vector<std::pair<Ra__Node*, Ra__Node*>> markers; // markers, joins
    
    // 1.
    Ra__Node* it = root;
    find_subquery_markers(&it, markers, RA__JOIN__SEMI_LEFT_DEPENDENT);
    it = root;
    find_subquery_markers(&it, markers, RA__JOIN__ANTI_LEFT_DEPENDENT);

    // 2.
    it = root;
    find_joins_by_markers(&it, markers);

    // 3.
    for(auto& exists: markers){
        // assert each marker found corresponding join
        assert(exists.second!=nullptr);
        simple_exists_to_in(exists);
    };
}

void RaTree::find_subquery_markers(Ra__Node** it, std::vector<std::pair<Ra__Node*, Ra__Node*>>& markers, Ra__Join__JoinType marker_type){

    if((*it)->node_case==RA__NODE__SELECTION){
        auto sel = static_cast<Ra__Node__Selection*>(*it);
        *it = sel->predicate;
        find_subquery_markers(it, markers, marker_type);
    }

    else if((*it)->node_case==RA__NODE__BOOL_PREDICATE){
        auto bool_p = static_cast<Ra__Node__Bool_Predicate*>(*it);
        for(auto arg: bool_p->args){
            *it = arg;
            find_subquery_markers(it, markers, marker_type);
        }
        return;
    }

    else if((*it)->node_case==RA__NODE__PREDICATE){
        auto p = static_cast<Ra__Node__Predicate*>(*it);

        *it = p->left;
        find_subquery_markers(it, markers, marker_type);
            
        *it = p->right;
        find_subquery_markers(it, markers, marker_type);

        return;
    }

    else if((*it)->node_case==RA__NODE__WHERE_SUBQUERY_MARKER){
        auto marker_ = static_cast<Ra__Node__Where_Subquery_Marker*>(*it);
        if(marker_->type==marker_type){
            // add to markers
            markers.push_back({*it,nullptr});
        }
        return;
    }
    auto childNodes = (*it)->childNodes;
    for(auto child: childNodes){
        *it = child;
        find_subquery_markers(it, markers, marker_type);
    }
    return;
}

void RaTree::find_joins_by_markers(Ra__Node** it, std::vector<std::pair<Ra__Node*, Ra__Node*>>& markers){

    if((*it)->n_children==0){
        return;
    }
    
    if((*it)->node_case==RA__NODE__JOIN){
        auto join = static_cast<Ra__Node__Join*>(*it);
        for(auto& pair: markers){
            auto marker = static_cast<Ra__Node__Where_Subquery_Marker*>(pair.first);
            if(join->right_where_subquery_marker==marker){
                assert(join->type==marker->type);
                assert(pair.second==nullptr);
                pair.second = *it;
                break;
            }
        }
    }

    auto childNodes = (*it)->childNodes;
    for(auto child: childNodes){
        *it = child;
        find_joins_by_markers(it, markers);
    }
    return;
}

void RaTree::get_relations_aliases(Ra__Node** it, std::set<std::pair<std::string,std::string>>& relations_aliases){
    if((*it)->node_case==RA__NODE__JOIN){
        auto join = static_cast<Ra__Node__Join*>((*it));
        switch(join->type){
            case RA__JOIN__CROSS_PRODUCT:{
                get_relations_aliases(&(*it)->childNodes[0], relations_aliases);
                get_relations_aliases(&(*it)->childNodes[1], relations_aliases);
                break;
            }
            case RA__JOIN__LEFT:
            case RA__JOIN__INNER:{
                if(join->alias.length()==0){
                    std::cout << "join in subquery must have alias to find corresponding attributes" << std::endl;
                    break;
                }
                relations_aliases.insert({"",join->alias});
            }
            // else: joins produced from subqueries in where clause
            default: {
                // right side is subquery, continue search on left side
                get_relations_aliases(&(*it)->childNodes[0], relations_aliases);
            }
        }
        return;
    }

    if((*it)->node_case==RA__NODE__RELATION){
        auto rel = static_cast<Ra__Node__Relation*>((*it));
        relations_aliases.insert({rel->name,rel->alias});
        return;
    }

    auto childNodes = (*it)->childNodes;
    for(auto child: childNodes){
        *it = child;
        get_relations_aliases(it, relations_aliases);
    }
    return;
}

bool RaTree::get_selection_parent(Ra__Node** it){
    auto childNodes = (*it)->childNodes;
    for(auto child: childNodes){
        if(child->node_case==RA__NODE__SELECTION){
            return true;
        }
    }

    bool found = false;
    for(auto child: childNodes){
        if(!found){
            *it = child;
            found = get_selection_parent(it);
        }
    }
    return found;
}

bool RaTree::get_selection(Ra__Node** it){
    assert((*it)->n_children==1);

    if((*it)->node_case==RA__NODE__SELECTION){
        return true;
    }

    bool found = false;
    auto childNodes = (*it)->childNodes;
    for(auto child: childNodes){
        if(!found){
            *it = child;
            found = get_selection(it);
        }
    }
    return found;
}

bool is_tpch_attribute(std::string attr, std::string relation){
    std::string::size_type pos = attr.find('_');
    std::string attr_prefix;
    attr_prefix = attr.substr(0, pos);

    if(attr_prefix=="p" && relation=="part"
        || attr_prefix=="s" && relation=="supplier"
        || attr_prefix=="ps" && relation=="partsupp"
        || attr_prefix=="c" && relation=="customer"
        || attr_prefix=="n" && relation=="nation"
        || attr_prefix=="l" && relation=="lineitem"
        || attr_prefix=="r" && relation=="region"
        || attr_prefix=="o" && relation=="orders")
    {
        return true;
    }
    return false;
}

// return {correlating attr, non-correlating attr}
std::pair<Ra__Node*,Ra__Node*> is_simply_correlating_predicate(Ra__Node__Predicate* p, const std::set<std::pair<std::string,std::string>>& relations_aliases ){
    if(p->left->node_case!=RA__NODE__ATTRIBUTE || p->right->node_case!=RA__NODE__ATTRIBUTE){
        // decorrelating exists only supports simple predicates 
        return {nullptr,nullptr};
    }
    // simple correlation uses equi predicate
    if(p->binaryOperator!="="){
        return {nullptr,nullptr};
    }
    // check left and right to find out if is correlating predicate
    bool left_found_relation = false;
    auto left_attr = static_cast<Ra__Node__Attribute*>(p->left);
    for(auto relation_alias: relations_aliases){
        // if alias exists, check if same as relation name or alias
        if(left_attr->alias.length()>0 && (left_attr->alias==relation_alias.first || left_attr->alias==relation_alias.second)){
            left_found_relation = true;
        }
        // attribute and relation no alias -> check if is tpch attribute of relation
        else if(left_attr->alias.length()==0 && relation_alias.second.length()==0 && is_tpch_attribute(left_attr->name,relation_alias.first)){
            left_found_relation = true;
        }
    }

    bool right_found_relation = false;
    auto right_attr = static_cast<Ra__Node__Attribute*>(p->right);
    for(auto relation_alias: relations_aliases){
        // if alias exists, check if same as relation name or alias
        if(right_attr->alias.length()>0 && (right_attr->alias==relation_alias.first || right_attr->alias==relation_alias.second)){
            right_found_relation = true;
        }
        // attribute and relation no alias -> check if is tpch attribute of relation
        else if(right_attr->alias.length()==0 && relation_alias.second.length()==0 && is_tpch_attribute(right_attr->name,relation_alias.first)){
            right_found_relation = true;
        }
    }

    // both side of predicate are correlating (outer) attributes, not supported
    if(!right_found_relation && !left_found_relation){
        std::cout << "predicates with both sides correlating is not supported" << std::endl;
        return {nullptr, nullptr};
    }
    else if(!right_found_relation){
        return {p->right, p->left};
    }
    else{ // !left_found_relation
        return {p->left, p->right};
    }
}

// exists predicates must be simple <col> <operator> <col>
void RaTree::simple_exists_to_in(std::pair<Ra__Node*, Ra__Node*>& exists){
    // 1. check if subquery simply correlated

    // go through tree, find join and relation aliases/names
    std::set<std::pair<std::string,std::string>> relations_aliases;
    Ra__Node* it = (exists.second)->childNodes[1];
    get_relations_aliases(&it, relations_aliases);

    // find (first & only) selection
    it = (exists.second)->childNodes[1];
    // correlated exists must have selection
    assert(get_selection(&it));
    Ra__Node__Selection* sel = static_cast<Ra__Node__Selection*>(it);

    // check top level selection predicates (predicate/bool AND) for: 
    // equi predicate with one attribute from outside
    // attributes must use alias or use tpch naming
    std::vector<std::tuple<Ra__Node*,Ra__Node*,size_t>> correlating_predicates;
    bool is_boolean_predicate = false;
    switch(sel->predicate->node_case){
        case RA__NODE__BOOL_PREDICATE:{
            is_boolean_predicate = true;
            auto bool_p = static_cast<Ra__Node__Bool_Predicate*>(sel->predicate);
            // only boolean "and" query can be simple correlation 
            if(bool_p->bool_operator==RA__BOOL_OPERATOR__AND){
                for(size_t i=0; i<bool_p->args.size(); i++){
                    if(bool_p->args[i]->node_case==RA__NODE__PREDICATE){
                        auto p = static_cast<Ra__Node__Predicate*>(bool_p->args[i]);
                        std::pair<Ra__Node*,Ra__Node*> cor_predicate = is_simply_correlating_predicate(p, relations_aliases);
                        if(cor_predicate.first!=nullptr){
                            correlating_predicates.push_back(std::make_tuple(cor_predicate.first,cor_predicate.second,i));
                        }
                    }
                }
            }
            break;
        }
        case RA__NODE__PREDICATE:{
            auto p = static_cast<Ra__Node__Predicate*>(sel->predicate);
            std::pair<Ra__Node*,Ra__Node*> cor_predicate = is_simply_correlating_predicate(p, relations_aliases);
            if(cor_predicate.first!=nullptr){
                correlating_predicates.push_back(std::make_tuple(cor_predicate.first,cor_predicate.second,0));
            }
            break;
        }
        default: std::cout << "selection should not have this node case as predicate" << std::endl;
    }
    if(correlating_predicates.size()!=1){
        // not simply correlated
        return;
    }

    auto foo = static_cast<Ra__Node__Attribute*>(std::get<0>(correlating_predicates[0]));
    auto foo2 = static_cast<Ra__Node__Attribute*>(std::get<1>(correlating_predicates[0]));
    std::cout << foo->to_string() << "=" << foo2->to_string() << std::endl;
    
    // 2. remove correlating predicate from subquery selection
    // if single predicate, remove selection node
    if(!is_boolean_predicate){
        it = (exists.second)->childNodes[1];
        get_selection_parent(&it);
        it->childNodes[0] = sel->childNodes[0];
        sel->childNodes.pop_back();
        delete sel;
    }
    // if part of "and" boolean predicate
    else{
        // if only 2 predicates
        auto bool_p = static_cast<Ra__Node__Bool_Predicate*>(sel->predicate);
        assert(bool_p->args.size()>1);
        bool_p->args.erase(bool_p->args.begin() + std::get<2>(correlating_predicates[0]));
        if(bool_p->args.size()==2){
            // change boolean to normal predicate
            sel->predicate = bool_p->args[0];
            bool_p->args.pop_back();
            delete bool_p;
        }
    }

    // 3. set subquery select target to inner predicate attribute
    auto pr = static_cast<Ra__Node__Projection*>((exists.second)->childNodes[1]);
    assert(pr->args.size()==1);
    delete pr->args[0];
    pr->args[0] = std::get<1>(correlating_predicates[0]);

    // check if "exists" or "not exists"
    auto join = static_cast<Ra__Node__Join*>(exists.second);
    Ra__Join__JoinType newType;
    if(join->type==RA__JOIN__SEMI_LEFT_DEPENDENT){
        newType = RA__JOIN__IN_LEFT;
    }
    else if(join->type==RA__JOIN__ANTI_LEFT_DEPENDENT){
        newType = RA__JOIN__ANTI_IN_LEFT;
    }
    else{
        std::cout << "this should be unreachable :)" << std::endl;
        return;
    }

    // 4. change "exists"/"not exists" join to "in"/"not in" join
    join->type = newType;
    // set predicate attr to "in" probe (join->predicate->left)
    auto p = new Ra__Node__Predicate();
    p->left = std::get<0>(correlating_predicates[0]);
    join->predicate = p;
    
    // 5. change selection marker type from "exists" to "in"
    auto marker = static_cast<Ra__Node__Where_Subquery_Marker*>(exists.first);
    marker->type = newType;

    return;
}

// void get_relations(Ra__Node** it, std::vector<Ra__Node**>& relations){
//     if((*it)->n_children == 0){
//         assert((*it)->node_case==RA__NODE__RELATION);
//         relations.push_back(it);
//         return;
//     }

//     for(auto& child: (*it)->childNodes){
//         get_relations(&child, relations);
//     }
//     return;
// }