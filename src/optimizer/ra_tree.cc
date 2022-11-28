#include "ra_tree.h"
#include <tuple>
#include <unordered_map>

RaTree::RaTree(Ra__Node* _root, std::vector<Ra__Node*> _ctes, uint64_t _counter)
:root(_root), ctes(_ctes), counter(_counter){
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
    decorrelate_exists();
};

// 1. find all "exists" markers
// 2. find all "exists" joins belonging to markers
// 3. for each "exists", check if subquery simply correlated, decorrelate:
    // remove correlating predicate from subquery selection
    // set subquery select target to inner predicate attribute
    // change "exists" join to "in" join
        // set predicate attr to "in" probe (join->predicate->left)
    // change selection marker type from "exists" to "in"
void RaTree::decorrelate_exists(){
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

    // 3. loop in reverse, to decorrelate most nested exists first
    for(int i=markers.size()-1; i>=0; i--){
        // assert each marker found corresponding join
        assert(markers[i].second!=nullptr);
        check_exists_correlation_type(markers[i]);
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

bool is_tpch_attribute(std::string attr_name, std::string relation){
    std::string::size_type pos = attr_name.find('_');
    std::string attr_prefix = attr_name.substr(0, pos);

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

// return {outer attr, inner attr, operator}
std::tuple<Ra__Node*,Ra__Node*,std::string> is_correlating_predicate(Ra__Node__Predicate* p, const std::set<std::pair<std::string,std::string>>& relations_aliases ){
    if(p->left->node_case!=RA__NODE__ATTRIBUTE || p->right->node_case!=RA__NODE__ATTRIBUTE){
        // if either side is an const -> not correlated
        // only supporting simple attribute, no expression with attribute
        return {nullptr,nullptr,""};
    }

    // check left and right to find out if is correlating predicate
    bool left_found_relation = false;
    auto left_attr = static_cast<Ra__Node__Attribute*>(p->left);
    for(auto relation_alias: relations_aliases){
        // if alias exists, check if same as relation name or relation alias
        if(left_attr->alias.length()>0 && (left_attr->alias==relation_alias.first || left_attr->alias==relation_alias.second)){
            left_found_relation = true;
            break;
        }
        // attribute and relation no alias -> check if is tpch attribute of relation
        else if(left_attr->alias.length()==0 && relation_alias.second.length()==0 && is_tpch_attribute(left_attr->name,relation_alias.first)){
            left_found_relation = true;
            break;
        }
    }

    bool right_found_relation = false;
    auto right_attr = static_cast<Ra__Node__Attribute*>(p->right);
    for(auto relation_alias: relations_aliases){
        // if alias exists, check if same as relation name or relation alias
        if(right_attr->alias.length()>0 && (right_attr->alias==relation_alias.first || right_attr->alias==relation_alias.second)){
            right_found_relation = true;
            break;
        }
        // attribute and relation no alias -> check if is tpch attribute of relation
        else if(right_attr->alias.length()==0 && relation_alias.second.length()==0 && is_tpch_attribute(right_attr->name,relation_alias.first)){
            right_found_relation = true;
            break;
        }
    }

    // not correlating predicate
    if(right_found_relation && left_found_relation){
        return {nullptr, nullptr,""};
    }
    // both side of predicate are correlating (outer) attributes, not supported
    else if(!right_found_relation && !left_found_relation){
        std::cout << "predicates with both sides correlating is not supported" << std::endl;
        return {nullptr, nullptr,""};
    }
    // return {correlating attr, non-correlating attr}
    else if(!right_found_relation){
        return {p->right, p->left, p->binaryOperator};
    }
    else{ // !left_found_relation
        return {p->left, p->right, p->binaryOperator};
    }
}

void RaTree::decorrelate_exists_trivial(bool is_boolean_predicate, std::tuple<Ra__Node*,Ra__Node*,std::string,size_t> correlating_predicate, Ra__Node__Selection* sel, std::pair<Ra__Node*, Ra__Node*> exists){
auto foo = static_cast<Ra__Node__Attribute*>(std::get<0>(correlating_predicate));
    auto foo2 = static_cast<Ra__Node__Attribute*>(std::get<1>(correlating_predicate));
    std::cout << foo->to_string() << "=" << foo2->to_string() << std::endl;

    // 2. remove correlating predicate from subquery selection
    // if single predicate, remove selection node
    if(!is_boolean_predicate){
        Ra__Node* it = (exists.second)->childNodes[1];
        get_selection_parent(&it);
        it->childNodes[0] = sel->childNodes[0];
        sel->childNodes.pop_back();
        delete sel;
    }
    // if part of boolean predicate
    else{
        auto bool_p = static_cast<Ra__Node__Bool_Predicate*>(sel->predicate);
        assert(bool_p->args.size()>1);
        bool_p->args.erase(bool_p->args.begin() + std::get<3>(correlating_predicate));
        // if only 2 predicates, convert boolean to single normal predicate
        if(bool_p->args.size()==2){
            sel->predicate = bool_p->args[0];
            bool_p->args.pop_back();
            delete bool_p;
        }
    }

    // 3. set subquery select target to inner predicate attribute
    auto pr = static_cast<Ra__Node__Projection*>((exists.second)->childNodes[1]);
    assert(pr->args.size()==1);
    delete pr->args[0];
    pr->args[0] = std::get<1>(correlating_predicate);

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
    p->left = std::get<0>(correlating_predicate);
    join->predicate = p;
    
    // 5. change selection marker type from "exists" to "in"
    auto marker = static_cast<Ra__Node__Where_Subquery_Marker*>(exists.first);
    marker->type = newType;
}

void RaTree::extract_predicates(Ra__Node* it, std::vector<std::tuple<Ra__Node*,Ra__Node*,std::string,size_t>>& correlating_predicates, bool& is_boolean_predicate, std::set<std::pair<std::string,std::string>> relations_aliases){
    switch(it->node_case){
        case RA__NODE__BOOL_PREDICATE:{
            is_boolean_predicate = true;
            auto bool_p = static_cast<Ra__Node__Bool_Predicate*>(it);
                for(auto p: bool_p->args){
                    extract_predicates(p, correlating_predicates, is_boolean_predicate, relations_aliases);
                }
            break;
        }
        case RA__NODE__PREDICATE:{
            auto p = static_cast<Ra__Node__Predicate*>(it);
            std::tuple<Ra__Node*,Ra__Node*,std::string> cor_predicate = is_correlating_predicate(p, relations_aliases);
            if(std::get<0>(cor_predicate)!=nullptr){
                correlating_predicates.push_back(std::make_tuple(std::get<0>(cor_predicate),std::get<1>(cor_predicate),std::get<2>(cor_predicate),0));
            }
            break;
        }
        default: std::cout << "This node case should not be in selection predicate" << std::endl;
    }
}

// exists predicates must be simple <col> <operator> <col>
void RaTree::check_exists_correlation_type(std::pair<Ra__Node*, Ra__Node*> exists){
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

    std::vector<std::tuple<Ra__Node*,Ra__Node*,std::string,size_t>> correlating_predicates;
    bool is_boolean_predicate = false;
    extract_predicates(sel->predicate, correlating_predicates, is_boolean_predicate, relations_aliases);
    
    if(is_trivially_correlated(correlating_predicates)){
        decorrelate_exists_trivial(is_boolean_predicate, correlating_predicates[0], sel, exists);
    }
    else{
        decorrelate_exists_complex(correlating_predicates, exists);
    }

    return;
}

// only supports refering to base tpch relations
Ra__Node* build_tpch_relation(std::string alias, std::vector<Ra__Node__Attribute*> attr){
    
    std::string tpch_relation_name = "";

    std::string::size_type pos = attr[0]->name.find('_');
    std::string attr_prefix = attr[0]->name.substr(0, pos);

    if(attr_prefix=="p"){
        tpch_relation_name = "part";
    }
    else if(attr_prefix=="s"){
        tpch_relation_name = "supplier";
    } 
    else if(attr_prefix=="ps"){
        tpch_relation_name = "partsupp";
    }
    else if(attr_prefix=="c"){
        tpch_relation_name = "customer";
    }
    else if(attr_prefix=="n"){
        tpch_relation_name = "nation";
    }
    else if(attr_prefix=="l"){
        tpch_relation_name = "lineitem";
    }
    else if(attr_prefix=="r"){
        tpch_relation_name = "region";
    }
    else if(attr_prefix=="o"){
        tpch_relation_name = "orders";
    }
    else{
        std::cout << "correlated attribute does not belong to tpch relation" << std::endl;
        tpch_relation_name = "error";
    }

    auto r = new Ra__Node__Relation(tpch_relation_name, alias);
    
    return r;
}

bool get_join(Ra__Node** it){
    if((*it)->node_case==RA__NODE__JOIN){
        return true;
    }

    bool found = false;
    auto childNodes = (*it)->childNodes;
    for(auto child: childNodes){
        if(!found){
            *it = child;
            found = get_join(it);
        }
    }
    return found;
}

bool get_join_parent(Ra__Node** it){
    auto childNodes = (*it)->childNodes;
    for(auto child: childNodes){
        if(child->node_case==RA__NODE__JOIN || child->node_case==RA__NODE__RELATION){
            return true;
        }
    }

    bool found = false;
    for(auto child: childNodes){
        if(!found){
            *it = child;
            found = get_join_parent(it);
        }
    }
    return found;
}

bool get_join_marker_parent(Ra__Node** it, Ra__Node__Where_Subquery_Marker* marker, int& index){
    auto childNodes = (*it)->childNodes;
    for(size_t i=0; i<(*it)->n_children; i++){
        if(childNodes[i]->node_case==RA__NODE__JOIN && static_cast<Ra__Node__Join*>(childNodes[i])->right_where_subquery_marker==marker){
            index = i;
            return true;
        }
    }

    bool found = false;
    for(auto child: childNodes){
        if(!found){
            *it = child;
            found = get_join_marker_parent(it, marker, index);
        }
    }
    return found;
}

bool find_marker_parent(Ra__Node** it, Ra__Node__Where_Subquery_Marker* marker, int& index){

    if((*it)->node_case==RA__NODE__SELECTION){
        auto sel = static_cast<Ra__Node__Selection*>(*it);
        if(sel->predicate->node_case==RA__NODE__WHERE_SUBQUERY_MARKER && static_cast<Ra__Node__Where_Subquery_Marker*>(sel->predicate)==marker){
            return true;
        }
        // if marker is part of boolean "not", then return grandparent of marker
        else if(sel->predicate->node_case==RA__NODE__BOOL_PREDICATE){
            auto bool_p = static_cast<Ra__Node__Bool_Predicate*>(*it);
            if(bool_p->bool_operator==RA__BOOL_OPERATOR__NOT){
                for(size_t i=0; i<bool_p->args.size(); i++){
                    if(bool_p->args[i]->node_case==RA__NODE__WHERE_SUBQUERY_MARKER && static_cast<Ra__Node__Where_Subquery_Marker*>(bool_p->args[i])==marker){
                        index = i;
                        return true;
                    }
                }
            }
        }
        *it = sel->predicate;
        return find_marker_parent(it, marker, index);
    }

    else if((*it)->node_case==RA__NODE__BOOL_PREDICATE){
        auto bool_p = static_cast<Ra__Node__Bool_Predicate*>(*it);
        for(size_t i=0; i<bool_p->args.size(); i++){
            if(bool_p->args[i]->node_case==RA__NODE__WHERE_SUBQUERY_MARKER && static_cast<Ra__Node__Where_Subquery_Marker*>(bool_p->args[i])==marker){
                index = i;
                return true;
            }
            // if marker is part of boolean "not", then return grandparent of marker
            else if(bool_p->args[i]->node_case==RA__NODE__BOOL_PREDICATE){
                auto bool_not_p = static_cast<Ra__Node__Bool_Predicate*>(bool_p->args[i]);
                if(bool_not_p->bool_operator==RA__BOOL_OPERATOR__NOT){
                    for(size_t j=0; j<bool_not_p->args.size(); j++){
                        if(bool_not_p->args[j]->node_case==RA__NODE__WHERE_SUBQUERY_MARKER && static_cast<Ra__Node__Where_Subquery_Marker*>(bool_not_p->args[j])==marker){
                            index = i;
                            return true;
                        }
                    }
                }
            }
        }
        bool found = false;
        for(auto arg: bool_p->args){
            if(!found){
                *it = arg;
                found = find_marker_parent(it, marker, index);
            }
        }
        return found;
    }

    auto childNodes = (*it)->childNodes;
    bool found = false;
    for(auto child: childNodes){
        if(!found){
            *it = child;
            found = find_marker_parent(it, marker, index);
        }
    }
    return found;
}

void RaTree::decorrelate_exists_complex(std::vector<std::tuple<Ra__Node*,Ra__Node*,std::string,size_t>> correlating_predicates, std::pair<Ra__Node*, Ra__Node*> exists){
    // move subquery to cte, give alias and columns (correlating attributes)
    auto exists_join = static_cast<Ra__Node__Join*>(exists.second);
    auto cte_subquery = static_cast<Ra__Node__Projection*>(exists_join->childNodes[1]);
    std::string cte_name = "m" + std::to_string(counter++);
    cte_subquery->subquery_alias = cte_name;

    // select distinct correlating attributes
    cte_subquery->distinct = true;
    for(auto arg: cte_subquery->args){
        delete arg;
    }
    cte_subquery->args.clear();
    for(auto p: correlating_predicates){
        auto attr = static_cast<Ra__Node__Attribute*>(std::get<0>(p));
        cte_subquery->subquery_columns.push_back(attr->name);
        cte_subquery->args.push_back(attr);
    }

    // list all correlating relations (using tpch name check of correlated attributes)
    std::unordered_map<std::string, std::vector<Ra__Node__Attribute*>> alias_attr_map; // map relation (identifier) to correlating attributes
    for(auto p: correlating_predicates){
        // check correlating attribute
        auto attr = static_cast<Ra__Node__Attribute*>(std::get<0>(p));

        if(attr->alias.length()>0){
            auto it = alias_attr_map.find(attr->alias);
            if (it != alias_attr_map.end()){
                alias_attr_map[attr->alias].push_back(attr);
            }
            else{
                alias_attr_map[attr->alias] = {attr};
            }
        }
        // correlating attribute has no alias
        else{
            std::string::size_type pos = attr->name.find('_');
            std::string attr_prefix = attr->name.substr(0, pos);
            auto it = alias_attr_map.find(attr_prefix);
            if (it != alias_attr_map.end()){
                alias_attr_map[attr_prefix].push_back(attr);
            }
            else{
                alias_attr_map[attr_prefix] = {attr};
            }
        }
    }

    std::vector<Ra__Node*> correlated_relations;
    for(auto alias_attr: alias_attr_map){
        correlated_relations.push_back(build_tpch_relation(alias_attr.first, alias_attr.second));
    }

    // add correlated relations to subquery
    Ra__Node* it = cte_subquery;
    assert(get_join_parent(&it));
    for(auto r: correlated_relations){
        assert(it->n_children==1);
        Ra__Node__Join* cp = new Ra__Node__Join(RA__JOIN__CROSS_PRODUCT);
        cp->childNodes.push_back(it->childNodes[0]);
        cp->childNodes.push_back(r);
        it->childNodes[0] = cp;
    }
    
    ctes.push_back(cte_subquery);

    // main tree: left join cte on correlating attributes (equi join)
    it = root;
    assert(get_join_parent(&it));
    Ra__Node__Join* left_join = new Ra__Node__Join(RA__JOIN__LEFT); 
    Ra__Node__Relation* cte_relation = new Ra__Node__Relation(cte_name); 
    left_join->childNodes.push_back(it->childNodes[0]);
    left_join->childNodes.push_back(cte_relation);
    if(correlating_predicates.size()==1){
        Ra__Node__Predicate* join_p = new Ra__Node__Predicate();
        auto cor_predicate = correlating_predicates[0];
        auto l_attr = static_cast<Ra__Node__Attribute*>(std::get<0>(cor_predicate));
        auto r_attr = static_cast<Ra__Node__Attribute*>(std::get<0>(cor_predicate));
        join_p->binaryOperator = "=";
        join_p->left = new Ra__Node__Attribute(l_attr->name, cte_name);
        join_p->right = new Ra__Node__Attribute(r_attr->name, r_attr->alias);
        left_join->predicate = join_p;
    }
    else{
        Ra__Node__Bool_Predicate* join_p = new Ra__Node__Bool_Predicate();
        join_p->bool_operator = RA__BOOL_OPERATOR__AND;
        for(auto cor_predicate: correlating_predicates){
            Ra__Node__Predicate* p = new Ra__Node__Predicate();
            auto l_attr = static_cast<Ra__Node__Attribute*>(std::get<0>(cor_predicate));
            auto r_attr = static_cast<Ra__Node__Attribute*>(std::get<0>(cor_predicate));
            p->binaryOperator = "=";
            p->left = new Ra__Node__Attribute(l_attr->name, cte_name);
            p->right = new Ra__Node__Attribute(r_attr->name, r_attr->alias);
            join_p->args.push_back(p);
        }
        left_join->predicate = join_p;
    }
    
    it->childNodes[0] = left_join;

    // in main selection: replace exists with null check
    // remove exists join
    it = root;
    int index = -1;
    assert(get_join_marker_parent(&it, exists_join->right_where_subquery_marker, index));
    Ra__Node* temp = it->childNodes[index];
    it->childNodes[index] = it->childNodes[index]->childNodes[0];
    temp->childNodes.clear();
    delete temp;

    // delete exists marker from main selection, replace with null check
    it = root;
    auto marker = static_cast<Ra__Node__Where_Subquery_Marker*>(exists.first);
    assert(find_marker_parent(&it, marker, index));
    Ra__Node__Null_Test* null_test = new Ra__Node__Null_Test();
    switch(marker->type){
        case RA__JOIN__SEMI_LEFT_DEPENDENT:{
            null_test->type = RA__NULL_TEST__IS_NOT_NULL;
            break;
        }
        case RA__JOIN__ANTI_LEFT_DEPENDENT:{
            null_test->type = RA__NULL_TEST__IS_NULL;
            break;
        }
        default: std::cout << "this join type should not be found here" << std::endl;
    }
    // can use any of the correlating predicates for null check
    null_test->arg = new Ra__Node__Attribute(static_cast<Ra__Node__Attribute*>(std::get<0>(correlating_predicates[0]))->name, cte_name);;
    
    switch(it->node_case){
        case RA__NODE__SELECTION:{
            auto sel = static_cast<Ra__Node__Selection*>(it);
            delete sel->predicate;
            sel->predicate = null_test;
            break;
        }
        case RA__NODE__BOOL_PREDICATE:{
            auto bool_p = static_cast<Ra__Node__Bool_Predicate*>(it);
            delete bool_p->args[index];
            bool_p->args[index] = null_test;
            break;
        }
        default: std::cout << "This node type should not be parent of marker" << std::endl;
    }
}

bool RaTree::is_trivially_correlated(std::vector<std::tuple<Ra__Node*,Ra__Node*,std::string,size_t>> correlating_predicates){
    if(correlating_predicates.size()==1 && (std::get<2>(correlating_predicates[0])=="=" || std::get<2>(correlating_predicates[0])=="<>")){
        return true;
    }
    return false;
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