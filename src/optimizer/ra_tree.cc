#include "ra_tree.h"
#include <tuple>
#include <unordered_map>
#include <map>

RaTree::RaTree(Ra__Node* _root, std::vector<Ra__Node*> _ctes, uint64_t _counter)
:root(_root), ctes(_ctes), counter(_counter){
}

RaTree::~RaTree(){
    delete root;
    for(auto cte: ctes){
        delete cte;
    }
}

void RaTree::optimize(){
    push_down_predicates();
    // decorrelate_all_exists();
    decorrelate_arbitrary_queries();
}

void RaTree::push_down_predicates(){  

    // 1. extract splittable predicates of all selections
        // 1.1 find all selections in tree, for each selection:
        // 1.2 split predicates 
        // 1.3 for each predicate: find all relations needed
        // 1.4 pop predicate
        // 1.5 save pointer to selection and all extracted predicates
    // 2. find new node for predicates
        // 2.1 in subtree of original selection: if find a lower node has all relations defined:
            // add predicate above node, remove from original selection
        // 2.2 if no lower node found with relations defined, re-add predicate to selection as boolean AND
        // 2.3 if all predicates have new node, remove old selection

    // 1.1
    std::vector<Ra__Node*> selections;
    get_all_selections(root, selections);

    // vector(selections),Ra__Node(sel child),vec(predicates),Ra__Node(predicate),vector(relations),string(rel_id)
    std::vector<std::pair<Ra__Node*,std::vector<std::pair<Ra__Node*,std::vector<std::string>>>>> selections_predicates_relations;
    for(const auto& selection: selections){
        auto sel = static_cast<Ra__Node__Selection*>(selection);
        std::vector<std::pair<Ra__Node*,std::vector<std::string>>> predicates_relations; // splitted predicates, and relations referenced

        // 1.2
        split_selection_predicates(sel->predicate, predicates_relations);

        // 1.3
        for(auto& p_r: predicates_relations){
            get_predicate_relations(p_r.first, p_r.second);
        }

        // 1.4 (will either find new node, or readded to this selection)
        sel->predicate = nullptr;

        // 1.5
        selections_predicates_relations.push_back({sel, predicates_relations});
    }

    // all selections are removed from tree
    // now insert extracted predicates at lowest node possible, add selection nodes as needed
    for(const auto& selection: selections_predicates_relations){
        for(const auto& predicate_relations: selection.second){
            // 2.1
            if(!find_lowest_child_insert_selection(predicate_relations, selection.first)){
                // 2.2
                add_predicate_to_selection(predicate_relations.first, selection.first);
            }
        }
        // 2.3
        if(static_cast<Ra__Node__Selection*>(selection.first)->predicate==nullptr){
            Ra__Node* sel_parent = root;
            int child_index = -1;
            get_node_parent(&sel_parent, selection.first, child_index);
            sel_parent->childNodes[child_index] = selection.first->childNodes[0];
            selection.first->childNodes.pop_back();
            delete selection.first;
        }
    }
}

void RaTree::add_predicate_to_selection(Ra__Node* predicate, Ra__Node* selection){
    auto sel = static_cast<Ra__Node__Selection*>(selection);
    if(sel->predicate==nullptr){
        sel->predicate = predicate;
    }
    else if(sel->predicate->node_case==RA__NODE__BOOL_PREDICATE){
        auto bool_p = static_cast<Ra__Node__Bool_Predicate*>(sel->predicate);
        if(bool_p->bool_operator==RA__BOOL_OPERATOR__AND){
            bool_p->args.push_back(predicate);
        }
        else{
            Ra__Node__Bool_Predicate* bool_p = new Ra__Node__Bool_Predicate();
            bool_p->bool_operator = RA__BOOL_OPERATOR__AND;
            bool_p->args.push_back(sel->predicate);
            bool_p->args.push_back(predicate);
            sel->predicate = bool_p;
        }
    }
    else if(sel->predicate->node_case==RA__NODE__PREDICATE){
        Ra__Node__Bool_Predicate* bool_p = new Ra__Node__Bool_Predicate();
        bool_p->bool_operator = RA__BOOL_OPERATOR__AND;
        bool_p->args.push_back(sel->predicate);
        bool_p->args.push_back(predicate);
        sel->predicate = bool_p;
    }
}

void RaTree::add_predicate_to_join(Ra__Node* predicate, Ra__Node* join_node){
    auto join = static_cast<Ra__Node__Join*>(join_node);
    if(join->predicate==nullptr){
        join->predicate = predicate;
    }
    else if(join->predicate->node_case==RA__NODE__BOOL_PREDICATE){
        auto bool_p = static_cast<Ra__Node__Bool_Predicate*>(join->predicate);
        if(bool_p->bool_operator==RA__BOOL_OPERATOR__AND){
            bool_p->args.push_back(predicate);
        }
        else{
            Ra__Node__Bool_Predicate* bool_p = new Ra__Node__Bool_Predicate();
            bool_p->bool_operator = RA__BOOL_OPERATOR__AND;
            bool_p->args.push_back(join->predicate);
            bool_p->args.push_back(predicate);
            join->predicate = bool_p;
        }
    }
    else if(join->predicate->node_case==RA__NODE__PREDICATE){
        Ra__Node__Bool_Predicate* bool_p = new Ra__Node__Bool_Predicate();
        bool_p->bool_operator = RA__BOOL_OPERATOR__AND;
        bool_p->args.push_back(join->predicate);
        bool_p->args.push_back(predicate);
        join->predicate = bool_p;
    }
}

bool RaTree::find_lowest_child_insert_selection(const std::pair<Ra__Node*,std::vector<std::string>>& predicate_relations, Ra__Node* selection){
    // keep pointer to parent, iterate through tree
    // from bottom, check relations and joins for which relations are defined
    // if all relations needed for predicate are defined, add selection above join/relation
        // if parent is already selection

    // 1. find if lower node has all relations defined
    Ra__Node* it = selection->childNodes[0];
    Ra__Node* child_it = nullptr;
    // how to deal with correlated predicates -> relations not defined in subtree...
    // ADDED: if new node found, add selection above
    if(find_node_where_relations_defined(&it, predicate_relations.second)){
        int child_index = -1;
        Ra__Node* parent = root;
        get_node_parent(&parent, it, child_index);
        // 2. insert selection
        if(parent->node_case==RA__NODE__SELECTION){
            // if parent is already selection, add predicate to it
            add_predicate_to_selection(predicate_relations.first, parent);
        }
        else{
            // insert new selection node
            Ra__Node__Selection* sel = new Ra__Node__Selection();
            sel->predicate = predicate_relations.first;
            sel->childNodes.push_back(parent->childNodes[child_index]);
            parent->childNodes[child_index] = sel;
        }
        return true;
    }
    return false;
}

bool RaTree::find_node_where_relations_defined(Ra__Node** it, const std::vector<std::string>& relations){
    
    // if relations defined here, check if also defined in children
    if(has_relations_defined(*it, relations)){
        bool found = false;
        Ra__Node* temp_this_node = *it;
        auto childNodes = (*it)->childNodes;
        for(int i=0; i<childNodes.size(); i++){
            if(!found){
                *it = childNodes[i];
                if(find_node_where_relations_defined(it, relations)){
                    found = true;
                }
            }
        }
        // current node defines relation, none of children do
        if(!found){
            *it = temp_this_node;
        }
        return true;
    }

    return false;
}

bool RaTree::has_relations_defined(Ra__Node* node, const std::vector<std::string>& relations){
    std::vector<std::pair<std::string,std::string>> defined_relations_aliases;
    get_relations_aliases(node, defined_relations_aliases);

    for(const auto& needed_relation: relations){
        bool is_defined = false;
        for(const auto& defined_relation: defined_relations_aliases){
            if(needed_relation==defined_relation.first || needed_relation==defined_relation.second){
                is_defined = true;
                break;
            }
        }
        if(!is_defined){
            return false;
        }
    }
    return true;
}

// fill relations vector with all relations referenced by predicate
void RaTree::get_predicate_relations(Ra__Node* predicate, std::vector<std::string>& relations){
    switch(predicate->node_case){
        case RA__NODE__PREDICATE:{
            auto p = static_cast<Ra__Node__Predicate*>(predicate);
            get_predicate_expression_relations(p->left, relations);
            get_predicate_expression_relations(p->right, relations);
            break;
        }
        case RA__NODE__WHERE_SUBQUERY_MARKER:{
            auto marker = static_cast<Ra__Node__Where_Subquery_Marker*>(predicate);
            get_predicate_expression_relations(marker, relations);
            break;
        }
        case RA__NODE__BOOL_PREDICATE:{
            auto bool_p = static_cast<Ra__Node__Bool_Predicate*>(predicate);
            for(const auto& arg: bool_p->args){
                get_predicate_relations(arg, relations);
            }
            break;
        }
        default: std::cout << "node case should not be in split predicates" << std::endl;
    }
    
}

void RaTree::get_predicate_expression_relations(Ra__Node* predicate_expr, std::vector<std::string>& relations){
    switch(predicate_expr->node_case){
        case RA__NODE__ATTRIBUTE:{
            relations.push_back(get_relation_from_attribute(predicate_expr));
            break;
        }
        case RA__NODE__EXPRESSION:{
            auto expr = static_cast<Ra__Node__Expression*>(predicate_expr);
            get_predicate_expression_relations(expr->l_arg, relations);
            get_predicate_expression_relations(expr->r_arg, relations);
            break;
        }
        case RA__NODE__FUNC_CALL:{
            auto func_call = static_cast<Ra__Node__Func_Call*>(predicate_expr);
            for(const auto& arg: func_call->args){
                get_predicate_expression_relations(arg, relations);
            }
            break;
        }
        case RA__NODE__TYPE_CAST:{
            auto type_cast = static_cast<Ra__Node__Type_Cast*>(predicate_expr);
            get_predicate_expression_relations(type_cast->expression, relations);
            break;
        }
        // if correlated subquery, need to return relations needed from outside
        case RA__NODE__WHERE_SUBQUERY_MARKER:{
            // get correlating attributes of subquery
            // get relation name/alias of correlating attributes

            // find join node through marker
            std::vector<std::pair<Ra__Node*,Ra__Node*>> markers_joins = {{predicate_expr,nullptr}};
            find_joins_by_markers(root, markers_joins);

            // go through subquery tree, find relation aliases/names
            std::vector<std::pair<std::string,std::string>> relations_aliases;
            Ra__Node* it = (markers_joins[0].second)->childNodes[1]->childNodes[0];
            // start searching from child of subquery projection
            get_relations_aliases(it, relations_aliases);

            // !This code only gets outer relations needed by correlated query, but we need all relations
            // // find (first & only) selection in correlated subquery
            // it = (markers_joins[0].second)->childNodes[1]->childNodes[0];
            // // if subquery has selection
            // if(get_first_selection(&it)){
            //     Ra__Node__Selection* sel = static_cast<Ra__Node__Selection*>(it);
            //     std::vector<std::tuple<Ra__Node*,Ra__Node*,std::string,size_t>> correlating_predicates;
            //     bool dummy_is_boolean_predicate = false;
            //     get_correlating_predicates(sel->predicate, correlating_predicates, dummy_is_boolean_predicate, relations_aliases);
            //     for(const auto& p: correlating_predicates){
            //         // get relation name of outer correlating attribute
            //         relations.push_back(get_relation_from_attribute(std::get<0>(p)));
            //     }
            // }
            // // no selection
            // else{
            //     break;
            // }

            for(auto relation_alias: relations_aliases){
                // if relation has alias
                if(relation_alias.second.length()>0){
                    relations.push_back(relation_alias.second);
                }
                // else add relation name
                else{
                    relations.push_back(relation_alias.first);
                }
            }
            break;
        }
        default: break; // const
    }
}

std::string RaTree::get_relation_from_attribute(Ra__Node* attribute){
    auto attr = static_cast<Ra__Node__Attribute*>(attribute);
    // if has alias, return alias
    if(attr->alias.length()>0){
        return attr->alias;
    }
    else{
        // if no alias, refer through tpch name
        return get_tpch_relation_name(attr->name);
    }
}

std::string RaTree::get_tpch_relation_name(std::string attr_name){
    std::string::size_type pos = attr_name.find('_');
    std::string attr_prefix = attr_name.substr(0, pos);

    if(attr_prefix=="p"){
        return "part";
    }
    else if(attr_prefix=="s"){
        return "supplier";
    }
    else if(attr_prefix=="ps"){
        return "partsupp";
    }
    else if(attr_prefix=="c"){
        return "customer";
    }
    else if(attr_prefix=="n"){
        return "nation";
    }
    else if(attr_prefix=="l"){
        return "lineitem";
    }
    else if(attr_prefix=="r"){
        return "region";
    }
    else if(attr_prefix=="o"){
        return "orders";
    }
    else{
        std::cout << "attribute is not tpch" << std::endl;
        return "";
    }
}

// split by "and"
void RaTree::split_selection_predicates(Ra__Node* predicate, std::vector<std::pair<Ra__Node*,std::vector<std::string>>>& predicates_relations){
    switch(predicate->node_case){
        case RA__NODE__WHERE_SUBQUERY_MARKER:
        case RA__NODE__PREDICATE:{
            predicates_relations.push_back({predicate,{}});
            break;
        }
        case RA__NODE__BOOL_PREDICATE:{
            auto bool_p = static_cast<Ra__Node__Bool_Predicate*>(predicate);
            switch(bool_p->bool_operator){
                case RA__BOOL_OPERATOR__AND:{
                    for(const auto& arg: bool_p->args){
                        split_selection_predicates(arg, predicates_relations);
                    }
                    break;
                }
                case RA__BOOL_OPERATOR__OR:
                case RA__BOOL_OPERATOR__NOT:{
                    // can't split predicate
                    predicates_relations.push_back({predicate,{}});
                    break;
                }
            }
            break;
        }
        default: std::cout << "this node case should not be in selection predicate" << std::endl;
    }
}

void RaTree::decorrelate_arbitrary_queries(){
    // find marker in selection
    std::vector<std::pair<Ra__Node*, Ra__Node*>> markers_joins; // markers, joins
    
    // 1.
    find_subquery_markers(root, markers_joins, RA__JOIN__DEPENDENT_INNER_LEFT);

    // 2.
    find_joins_by_markers(root, markers_joins);

    // 3. loop in reverse, to decorrelate most nested exists first
    for(int i=markers_joins.size()-1; i>=0; i--){
        // assert each marker found corresponding join
        assert(markers_joins[i].second!=nullptr);
        decorrelate_subquery(markers_joins[i]);
    };
}

void RaTree::decorrelate_subquery(std::pair<Ra__Node*, Ra__Node*> markers_joins){
    // 0. replace selection marker with produced attribute
    Ra__Node* it = root;
    auto marker = static_cast<Ra__Node__Where_Subquery_Marker*>(markers_joins.first);
    int child_index = -1;
    assert(find_marker_parent(&it, marker, child_index));
    replace_selection_marker(it, child_index, new Ra__Node__Attribute("m","t"));
    
    
    // 1.
    auto original_dep_join = static_cast<Ra__Node__Join*>(markers_joins.second);
    original_dep_join->type = RA__JOIN__INNER;
    original_dep_join->right_where_subquery_marker->marker = 0;

    // 1.1
    assert(original_dep_join->predicate==nullptr);

    // 1.2
    auto right_projection = static_cast<Ra__Node__Projection*>(original_dep_join->childNodes[1]);
    right_projection->subquery_alias = "t";
    right_projection->subquery_columns.push_back("m"); 

    // 2.
    Ra__Node__Join* dep_join = new Ra__Node__Join(RA__JOIN__DEPENDENT_INNER_LEFT);
    dep_join->childNodes.resize(2);
    dep_join->childNodes[1] = original_dep_join->childNodes[1];
    original_dep_join->childNodes[1] = dep_join;

    // 2.1
    Ra__Node__Projection* d_projection = new Ra__Node__Projection();
    d_projection->childNodes.push_back(original_dep_join->childNodes[0]);
    d_projection->subquery_alias = "d";
    dep_join->childNodes[0] = d_projection;
    auto correlated_attributes = intersect_correlated_attributes(dep_join);
    for(auto& correlated_attribute: correlated_attributes){
        auto attr = static_cast<Ra__Node__Attribute*>(correlated_attribute);
        // 2.1.2
        d_projection->args.push_back(new Ra__Node__Attribute(attr->name, attr->alias));
        d_projection->subquery_columns.push_back(attr->name);

        // 1.1 
        add_predicate_to_join(new Ra__Node__Predicate(new Ra__Node__Attribute(attr->name, attr->alias),new Ra__Node__Attribute(attr->name, "t"),"="), original_dep_join);

        // 2.2
        attr->alias = "d";
        right_projection->args.push_back(new Ra__Node__Attribute(attr->name, attr->alias));
        right_projection->subquery_columns.push_back(attr->name);
        
    }

    // 3.
    Ra__Node* dep_join_parent = original_dep_join;
    while(!intersect_correlated_attributes(dep_join).empty()){
        push_down_dep_join(&dep_join_parent);
    }

    // 3.1
    dep_join->type = RA__JOIN__CROSS_PRODUCT;

    // 4. 
    it = right_projection;
    assert(get_first_selection(&it));
    std::map<std::pair<std::string,std::string>, std::pair<std::string,std::string>> rename_d_attributes;
    if(can_decouple(it, d_projection->args, rename_d_attributes)){
        // 4.1
        // remove d projection args, pop child
        for(auto arg: d_projection->args){
            delete arg;
        }
        d_projection->args.clear();
        d_projection->childNodes.pop_back();
        delete d_projection;

        // remove join
        int dep_join_parent_child_id = 0;
        if(dep_join_parent->node_case==RA__NODE__JOIN){
            dep_join_parent_child_id = 1;
        }
        dep_join_parent->childNodes[dep_join_parent_child_id] = dep_join_parent->childNodes[dep_join_parent_child_id]->childNodes[1];
        dep_join->childNodes.clear();
        delete dep_join;

        // 4.2
        rename_attributes(right_projection, rename_d_attributes);
        // 4.3
        std::vector<Ra__Node*> selections;
        get_all_selections(right_projection, selections);
        for(auto& sel: selections){
            remove_redundant_predicates(sel, sel);
        }
    }
    // 5.
    else{
        // 5.1
        Ra__Node__Projection* cte_projection = new Ra__Node__Projection();
        cte_projection->args.push_back(new Ra__Node__Attribute("*"));
        cte_projection->childNodes.push_back(original_dep_join->childNodes[0]);
        std::string cte_name = "cte_" + std::to_string(counter++);
        cte_projection->subquery_alias = cte_name;
        ctes.push_back(cte_projection);

        // 5.2
        original_dep_join->childNodes[0] = new Ra__Node__Relation(cte_name);
        d_projection->childNodes[0] = original_dep_join->childNodes[0];

        // 5.3
        std::vector<std::pair<std::string, std::string>> cte_relations_aliases;
        get_relations_aliases(cte_projection->childNodes[0], cte_relations_aliases);

        // 5.4
        std::map<std::pair<std::string,std::string>, std::pair<std::string,std::string>> rename_map;
        for(const auto& cte_relation_alias: cte_relations_aliases){
            // no relation name, has alias
            if(cte_relation_alias.first==""){
                rename_map[{cte_relation_alias.second,""}] = {cte_name,""};
            }
            // no alias, has relation name
            else if(cte_relation_alias.second==""){
                rename_map[{cte_relation_alias.first,""}] = {cte_name,""};
            }
            // has relation name and alias
            else{
                rename_map[{cte_relation_alias.second,""}] = {cte_name,""};
            }
        }
        rename_attributes(root, rename_map);
    }

    // 0. Replace selection marker with produced attribute
    // 1. replace dep join with inner join, remove marker
        // 1.1 compute join predicate
        // 1.2 add alias and columns to right side projection
    // 2. add dep join to join right child
        // 2.1 compute left child D:
            // 2.1.1 subquery from top join left side child
            // 2.1.2 compute which attritbues to select -> add to d projection and right projection
        // 2.2 in right child, replace all outer query aliases with d
    // 3. push down dep join
        // 3.1 convert dep join to cp
    // 4. Decouple (if possible: only correlated equi predicates (for no, no expressions supported))
        // 4.1 remove join 
        // 4.2 rename all d attribute alias to their original alias (whole right side)
        // 4.3 remove a=a predicates
    // 5. If can't decouple, move to CTE
        // TODO: rename attributes to unique names in projection, create rename map for whole query
        // 5.1 create projection (select *) on left side, add to CTEs
        // 5.2 point d to CTE, point original dep join left to CTE
        // 5.3 get relations/aliases used in CTE
        // 5.4 go through whole tree, rename all attributes with CTE relations/alises
}

void RaTree::remove_redundant_predicates(Ra__Node* predicate, Ra__Node* predicate_parent){
    switch(predicate->node_case){
        case RA__NODE__SELECTION:{
            auto sel = static_cast<Ra__Node__Selection*>(predicate);
            remove_redundant_predicates(sel->predicate, predicate);
            break;
        }
        case RA__NODE__BOOL_PREDICATE:{
            auto bool_p = static_cast<Ra__Node__Bool_Predicate*>(predicate);
            for(auto& arg: bool_p->args){
                remove_redundant_predicates(arg, predicate);
            }
            break;
        }
        case RA__NODE__PREDICATE:{
            auto p = static_cast<Ra__Node__Predicate*>(predicate);
            if(p->left->node_case==RA__NODE__ATTRIBUTE && p->right->node_case==RA__NODE__ATTRIBUTE){
                auto attr_l = static_cast<Ra__Node__Attribute*>(p->left);
                auto attr_r = static_cast<Ra__Node__Attribute*>(p->right);
                if(attr_l->name==attr_r->name && attr_l->alias==attr_r->alias){
                    switch(predicate_parent->node_case){
                        case RA__NODE__SELECTION:{
                            // remove selection from tree
                            auto sel = static_cast<Ra__Node__Selection*>(predicate_parent);
                            Ra__Node* sel_parent = root;
                            int child_index = -1;
                            assert(get_node_parent(&sel_parent, sel, child_index));
                            sel_parent->childNodes[child_index] = sel->childNodes[0];
                            sel->childNodes.clear();
                            delete sel;
                            break;
                        }
                        case RA__NODE__BOOL_PREDICATE:{
                            auto bool_p = static_cast<Ra__Node__Bool_Predicate*>(predicate_parent);
                            for(size_t i=0; i<bool_p->args.size(); i++){
                                if(bool_p->args[i]==predicate){
                                    delete bool_p->args[i]; // == predicate
                                    bool_p->args.erase(bool_p->args.begin() + i);
                                    break;
                                }
                            }
                            // if only one predicate left, turn into normal predicate
                            if(bool_p->args.size()==1){
                                ;
                            }
                            break;
                        }
                        default: std::cout << "this node case should not be predicate parent" << std::endl;
                    }
                }
            }
            break;
        }
        default: return;
    }
}

// rename attributes in projection and selection
void RaTree::rename_attributes(Ra__Node* it, std::map<std::pair<std::string,std::string>, std::pair<std::string,std::string>> rename_map){
    switch(it->node_case){
        case RA__NODE__ATTRIBUTE:{
            auto attr = static_cast<Ra__Node__Attribute*>(it);
            auto rename = rename_map.find({attr->alias,attr->name});
            if(rename!=rename_map.end()){
                attr->alias = rename->second.first;
                attr->name = rename->second.second;
            }
            else{
                // rename alias of attribute
                auto rename_all = rename_map.find({attr->alias,""});
                if(rename_all!=rename_map.end()){
                    attr->alias = rename_all->second.first;
                }
            }
            break;
        }
        case RA__NODE__PROJECTION:{
            auto p = static_cast<Ra__Node__Projection*>(it);
            for(auto& arg: p->args){
                rename_attributes(arg, rename_map);
            }
            rename_attributes(p->childNodes[0], rename_map);
            break;
        }
        case RA__NODE__SELECTION:{
            auto sel = static_cast<Ra__Node__Selection*>(it);
            rename_attributes(sel->predicate, rename_map);
            rename_attributes(sel->childNodes[0], rename_map);
            break;
        }
        case RA__NODE__BOOL_PREDICATE:{
            auto bool_p = static_cast<Ra__Node__Bool_Predicate*>(it);
            for(auto& arg: bool_p->args){
                rename_attributes(arg, rename_map);
            }
            break;
        }
        case RA__NODE__PREDICATE:{
            auto p = static_cast<Ra__Node__Predicate*>(it);
            rename_attributes(p->left, rename_map);
            rename_attributes(p->right, rename_map);
            break;
        }
        case RA__NODE__GROUP_BY:{
            auto group_by = static_cast<Ra__Node__Group_By*>(it);
            for(auto& arg: group_by->args){
                rename_attributes(arg, rename_map);
            }
            rename_attributes(group_by->childNodes[0], rename_map);
            break;
        }
        case RA__NODE__JOIN:{
            auto join = static_cast<Ra__Node__Join*>(it);
            if(join->predicate!=nullptr){
                rename_attributes(join->predicate, rename_map);
            }
            rename_attributes(join->childNodes[0], rename_map);
            rename_attributes(join->childNodes[1], rename_map);
            break;
        }
        case RA__NODE__SELECT_EXPRESSION:{
            auto sel_expr = static_cast<Ra__Node__Select_Expression*>(it);
            rename_attributes(sel_expr->expression, rename_map);
            break;
        }
        default: return;
    }
}

// only has equivalent class if has an equi predicate in a top level "and" bool predicate
bool RaTree::has_equivalent_attribute(Ra__Node* attribute, Ra__Node* predicate, std::map<std::pair<std::string,std::string>, std::pair<std::string,std::string>>& d_rename_map){
    switch(predicate->node_case){
        case RA__NODE__BOOL_PREDICATE:{
            auto bool_p = static_cast<Ra__Node__Bool_Predicate*>(predicate);
            if(bool_p->bool_operator==RA__BOOL_OPERATOR__AND){
                bool found_equivalence_class = false;
                for(const auto& arg: bool_p->args){
                    if(!found_equivalence_class){
                        found_equivalence_class = has_equivalent_attribute(attribute, arg, d_rename_map);
                    }
                }
                return found_equivalence_class;
            }
            else{
                return false;
            }
        }
        case RA__NODE__PREDICATE:{
            auto p = static_cast<Ra__Node__Predicate*>(predicate);
            auto attr_d = static_cast<Ra__Node__Attribute*>(attribute);
            if(p->binaryOperator=="=" && p->left->node_case==RA__NODE__ATTRIBUTE && p->right->node_case==RA__NODE__ATTRIBUTE){
                auto attr_l = static_cast<Ra__Node__Attribute*>(p->left);
                auto attr_r = static_cast<Ra__Node__Attribute*>(p->right);
                if((attr_l->alias=="d" && attr_l->name==attr_d->name && attr_r->alias!="d")){
                    d_rename_map[{"d",attr_d->name}] = {attr_r->alias,attr_r->name};
                    return true;
                }
                else if((attr_r->alias=="d" && attr_r->name==attr_d->name && attr_l->alias!="d")){
                    d_rename_map[{"d",attr_d->name}] = {attr_l->alias,attr_l->name};
                    return true;
                }
            }
            return false;
        }
        default: return false;
    }
}

// can only decouple if every "d" attribute has an equi predicate in a top level "and" bool predicate
// (can also have any <,> predicates additionally)
bool RaTree::can_decouple(Ra__Node* selection, const std::vector<Ra__Node*>& d_args, std::map<std::pair<std::string,std::string>, std::pair<std::string,std::string>>& d_rename_map){
    auto sel = static_cast<Ra__Node__Selection*>(selection);
    // every attribute passed from "d" must have an equi predicate
    bool can_decouple = true;
    for(const auto& d_arg: d_args){
        if(!has_equivalent_attribute(d_arg, sel->predicate, d_rename_map)){
            return false;
        };
    }
    return true;
}

void RaTree::push_down_dep_join(Ra__Node** dep_join_parent){
    // switch dep join parent node case
    int dep_join_parent_child_id = 0;
    if((*dep_join_parent)->node_case==RA__NODE__JOIN){
        dep_join_parent_child_id = 1;
    }

    // switch dep join right child node case
    switch((*dep_join_parent)->childNodes[dep_join_parent_child_id]->childNodes[1]->node_case){
        // single child cases
        case RA__NODE__PROJECTION:
        case RA__NODE__SELECTION:{
            Ra__Node* dep_join = (*dep_join_parent)->childNodes[dep_join_parent_child_id];
            Ra__Node* dep_join_right_child = dep_join->childNodes[1];

            dep_join->childNodes[1] = dep_join_right_child->childNodes[0];
            dep_join_right_child->childNodes[0] = dep_join;
            (*dep_join_parent)->childNodes[dep_join_parent_child_id] = dep_join_right_child;
            break;
        }
        case RA__NODE__GROUP_BY:{
            Ra__Node* dep_join = (*dep_join_parent)->childNodes[dep_join_parent_child_id];
            Ra__Node* dep_join_right_child = dep_join->childNodes[1];

            // get attributes produced by d
            std::vector<std::string> d_attributes = static_cast<Ra__Node__Projection*>(dep_join->childNodes[0])->subquery_columns;
            for(const auto& d_attr: d_attributes){
                static_cast<Ra__Node__Group_By*>(dep_join_right_child)->args.push_back(new Ra__Node__Attribute(d_attr, "d"));
            }
            static_cast<Ra__Node__Group_By*>(dep_join_right_child)->implicit = false;

            dep_join->childNodes[1] = dep_join_right_child->childNodes[0];
            dep_join_right_child->childNodes[0] = dep_join;
            (*dep_join_parent)->childNodes[dep_join_parent_child_id] = dep_join_right_child;
            break;
        }
        case RA__NODE__JOIN:{
            break;
        }
    }
    *dep_join_parent = (*dep_join_parent)->childNodes[dep_join_parent_child_id];
}

// compute intersect of attributes produced by left with free attributes of right
std::vector<Ra__Node*> RaTree::intersect_correlated_attributes(Ra__Node* dep_join){
    std::vector<Ra__Node*> intersect_result; 

    // get relations of left side
    std::vector<std::pair<std::string, std::string>> left_relations_aliases;
    // if D projection has already been initialized
    auto left_projection = static_cast<Ra__Node__Projection*>(dep_join->childNodes[0]);
    if(left_projection->args.size()>0){
        left_relations_aliases.push_back({"","d"});
    }
    else{
        get_relations_aliases(left_projection->childNodes[0], left_relations_aliases);
    }

    // get attributes of right side
    std::vector<Ra__Node*> right_attributes;
    Ra__Node* dep_right_child = dep_join->childNodes[1];
    get_subtree_attributes(dep_right_child, right_attributes); // currently gets attributes from selection and projection 

    // for each right attribute, if it matches a left relation, return it
    for(const auto& attribute: right_attributes){
        auto attr = static_cast<Ra__Node__Attribute*>(attribute);
        bool found_left_relation = false;
        for(const auto& relation: left_relations_aliases){
            if(found_left_relation){
                break;
            }
            if(attr->alias.length()>0 && (attr->alias==relation.first || attr->alias==relation.second)){
                found_left_relation = true;
                intersect_result.push_back(attr);
            }
            else if(attr->alias=="" && is_tpch_attribute(attr->name, relation.first)){
                found_left_relation = true;
                intersect_result.push_back(attr);
            }
        }
    }

    return intersect_result;
}

void RaTree::get_expression_attributes(Ra__Node* expression, std::vector<Ra__Node*>& attributes){
    switch(expression->node_case){
        case RA__NODE__ATTRIBUTE:{
            auto attr = static_cast<Ra__Node__Attribute*>(expression);
            attributes.push_back(attr);
            break;
        }
        case RA__NODE__EXPRESSION:{
            auto expr = static_cast<Ra__Node__Expression*>(expression);
            if(expr->l_arg!=nullptr){
                get_expression_attributes(expr->l_arg, attributes);
            }
            if(expr->r_arg!=nullptr){
                get_expression_attributes(expr->r_arg, attributes);
            }
            break;
        }
        case RA__NODE__FUNC_CALL:{
            auto func_call = static_cast<Ra__Node__Func_Call*>(expression);
            for(const auto& arg: func_call->args){
                get_expression_attributes(arg, attributes);
            }
            break;
        }
        case RA__NODE__TYPE_CAST:{
            auto type_cast = static_cast<Ra__Node__Type_Cast*>(expression);
            get_expression_attributes(type_cast->expression, attributes);
            break;
        }
    }
}

void RaTree::get_predicate_attributes(Ra__Node* predicate, std::vector<Ra__Node*>& attributes){
    switch(predicate->node_case){
        case RA__NODE__BOOL_PREDICATE:{
            auto bool_p = static_cast<Ra__Node__Bool_Predicate*>(predicate);
            for(const auto& p: bool_p->args){
                get_predicate_attributes(p, attributes);
            }
            break;
        }
        case RA__NODE__PREDICATE:{
            auto p = static_cast<Ra__Node__Predicate*>(predicate);
            get_expression_attributes(p->left, attributes);
            get_expression_attributes(p->right, attributes);
            break;
        }
        case RA__NODE__WHERE_SUBQUERY_MARKER:{
            std::vector<std::pair<Ra__Node*, Ra__Node*>> marker_join = {{predicate,nullptr}};
            find_joins_by_markers(root, marker_join);
            get_subtree_attributes(marker_join[0].second, attributes);
            break;
        }
        default: std::cout << "Node case should not be in predicate" << std::endl;
    }
}

// get attributes used in selection
void RaTree::get_subtree_attributes(Ra__Node* it, std::vector<Ra__Node*>& attributes){
    switch(it->node_case){
        case RA__NODE__SELECTION:{
            auto sel = static_cast<Ra__Node__Selection*>(it);
            get_predicate_attributes(sel->predicate, attributes);
            break;
        }
        case RA__NODE__PROJECTION:{
            auto proj = static_cast<Ra__Node__Projection*>(it);
            for(const auto& arg: proj->args){
                get_expression_attributes(arg, attributes);
            }
            break;
        }
        case RA__NODE__JOIN:{
            auto join = static_cast<Ra__Node__Join*>(it);
            get_predicate_attributes(join->predicate, attributes);
            break;
        }
        case RA__NODE__GROUP_BY:{
            break;
        }
        // TODO: all other nodes...
    }
    
    for(const auto& child: it->childNodes){
        get_subtree_attributes(child, attributes);
    }
    return;
}

// 1. find all "exists" markers
// 2. find all "exists" joins belonging to markers
// 3. for each "exists", check if subquery simply correlated, decorrelate:
    // remove correlating predicate from subquery selection
    // set subquery select target to inner predicate attribute
    // change "exists" join to "in" join
        // set predicate attr to "in" probe (join->predicate->left)
    // change selection marker type from "exists" to "in"
void RaTree::decorrelate_all_exists(){
    // find marker in selection
    std::vector<std::pair<Ra__Node*, Ra__Node*>> markers_joins; // markers, joins
    
    // 1.
    find_subquery_markers(root, markers_joins, RA__JOIN__SEMI_LEFT_DEPENDENT);
    find_subquery_markers(root, markers_joins, RA__JOIN__ANTI_LEFT_DEPENDENT);

    // 2.
    find_joins_by_markers(root, markers_joins);

    // 3. loop in reverse, to decorrelate most nested exists first
    for(int i=markers_joins.size()-1; i>=0; i--){
        // assert each marker found corresponding join
        assert(markers_joins[i].second!=nullptr);
        decorrelate_exists_subquery(markers_joins[i]);
    };
}

void RaTree::find_subquery_markers(Ra__Node* it, std::vector<std::pair<Ra__Node*, Ra__Node*>>& markers_joins, Ra__Join__JoinType marker_type){

    if(it->node_case==RA__NODE__SELECTION){
        auto sel = static_cast<Ra__Node__Selection*>(it);
        find_subquery_markers(sel->predicate, markers_joins, marker_type);
    }

    else if(it->node_case==RA__NODE__BOOL_PREDICATE){
        auto bool_p = static_cast<Ra__Node__Bool_Predicate*>(it);
        for(const auto& arg: bool_p->args){
            find_subquery_markers(arg, markers_joins, marker_type);
        }
        return;
    }

    else if(it->node_case==RA__NODE__PREDICATE){
        auto p = static_cast<Ra__Node__Predicate*>(it);
        find_subquery_markers(p->left, markers_joins, marker_type);
        find_subquery_markers(p->right, markers_joins, marker_type);
        return;
    }

    else if(it->node_case==RA__NODE__WHERE_SUBQUERY_MARKER){
        auto marker_ = static_cast<Ra__Node__Where_Subquery_Marker*>(it);
        if(marker_->type==marker_type){
            // add to markers
            markers_joins.push_back({it,nullptr});
        }
        return;
    }
    for(const auto& child: it->childNodes){
        find_subquery_markers(child, markers_joins, marker_type);
    }
    return;
}

void RaTree::find_joins_by_markers(Ra__Node* it, std::vector<std::pair<Ra__Node*, Ra__Node*>>& markers_joins){

    if(it->n_children==0){
        return;
    }
    
    if(it->node_case==RA__NODE__JOIN){
        auto join = static_cast<Ra__Node__Join*>(it);
        for(auto& pair: markers_joins){
            auto marker = static_cast<Ra__Node__Where_Subquery_Marker*>(pair.first);
            if(join->right_where_subquery_marker==marker){
                assert(join->type==marker->type);
                assert(pair.second==nullptr);
                pair.second = it;
                break;
            }
        }
    }

    for(const auto& child: it->childNodes){
        find_joins_by_markers(child, markers_joins);
    }
    return;
}

void RaTree::get_relations_aliases(Ra__Node* it, std::vector<std::pair<std::string,std::string>>& relations_aliases){
    if(it->node_case==RA__NODE__JOIN){
        auto join = static_cast<Ra__Node__Join*>(it);

        // join with rename, only join alias visible in namespace
        if(join->alias.length()>0){
            relations_aliases.push_back({"",join->alias});
        }
        // right side is selection subquery, only recurse on left
        else if(join->right_where_subquery_marker->marker!=0){
            get_relations_aliases(it->childNodes[0], relations_aliases);
        }
        else{
            get_relations_aliases(it->childNodes[0], relations_aliases);
            get_relations_aliases(it->childNodes[1], relations_aliases);
        }
        return;
    } 

    if(it->node_case==RA__NODE__RELATION){
        auto rel = static_cast<Ra__Node__Relation*>(it);
        relations_aliases.push_back({rel->name,rel->alias});
        return;
    }

    // from subquery
    if(it->node_case==RA__NODE__PROJECTION){
        auto pr = static_cast<Ra__Node__Projection*>(it);
        relations_aliases.push_back({"",pr->subquery_alias});
        return;
    }

    for(const auto& child: it->childNodes){
        get_relations_aliases(child, relations_aliases);
    }
    return;
}

bool RaTree::get_node_parent(Ra__Node** it, Ra__Node* child_target, int& child_index){
    auto childNodes = (*it)->childNodes;
    // for(const auto& child: childNodes){
    for(int i=0; i<childNodes.size(); i++){
        if(childNodes[i]==child_target){
            child_index = i;
            return true;
        }
    }

    bool found = false;
    for(const auto& child: childNodes){
        if(!found){
            *it = child;
            found = get_node_parent(it, child_target, child_index);
        }
    }
    return found;
}

// bool RaTree::get_selection_parent(Ra__Node** it){
//     auto childNodes = (*it)->childNodes;
//     for(const auto& child: childNodes){
//         if(child->node_case==RA__NODE__SELECTION){
//             return true;
//         }
//     }

//     bool found = false;
//     for(const auto& child: childNodes){
//         if(!found){
//             *it = child;
//             found = get_selection_parent(it);
//         }
//     }
//     return found;
// }

void RaTree::get_all_selections(Ra__Node* it, std::vector<Ra__Node*>& selections){

    if(it->node_case==RA__NODE__SELECTION){
        selections.push_back(it);
    }

    for(const auto& child: it->childNodes){
        get_all_selections(child, selections);
    }
}

bool RaTree::get_first_selection(Ra__Node** it){

    if((*it)->node_case==RA__NODE__SELECTION){
        return true;
    }
    
    // auto childNodes = (*it)->childNodes[0];
    // *it = childNode;
    // return get_first_selection(it);

    bool found = false;
    auto childNodes = (*it)->childNodes;
    for(const auto& child: childNodes){
        if(!found){
            *it = child;
            found = get_first_selection(it);
        }
    }
    return found;
}

bool RaTree::is_tpch_attribute(std::string attr_name, std::string relation){
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
std::tuple<Ra__Node*,Ra__Node*,std::string> RaTree::is_correlating_predicate(Ra__Node__Predicate* p, const std::vector<std::pair<std::string,std::string>>& relations_aliases){
    if(p->left->node_case!=RA__NODE__ATTRIBUTE || p->right->node_case!=RA__NODE__ATTRIBUTE){
        // if either side is an const -> not correlated
        // only supporting simple correlated predicate (<attr><op><attr>), no expression with attribute
        return {nullptr,nullptr,""};
    }

    // check left and right to find out if is correlating predicate
    bool left_found_relation = false;
    auto left_attr = static_cast<Ra__Node__Attribute*>(p->left);
    for(const auto& relation_alias: relations_aliases){
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
    for(const auto& relation_alias: relations_aliases){
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

void RaTree::decorrelate_exists_trivial(bool is_boolean_predicate, std::tuple<Ra__Node*,Ra__Node*,std::string,size_t> correlating_predicate, Ra__Node__Selection* sel, std::pair<Ra__Node*, Ra__Node*> markers_joins){
auto foo = static_cast<Ra__Node__Attribute*>(std::get<0>(correlating_predicate));
    auto foo2 = static_cast<Ra__Node__Attribute*>(std::get<1>(correlating_predicate));
    std::cout << foo->to_string() << "=" << foo2->to_string() << std::endl;

    // 2. remove correlating predicate from subquery selection
    // if single predicate, remove selection node
    if(!is_boolean_predicate){
        Ra__Node* it = (markers_joins.second)->childNodes[1];
        // get_selection_parent(&it);
        int child_index = -1;
        get_node_parent(&it, sel, child_index);
        assert(it->n_children==1 && child_index==0); // parent should have single child
        it->childNodes[child_index] = sel->childNodes[0];
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
    auto pr = static_cast<Ra__Node__Projection*>((markers_joins.second)->childNodes[1]);
    assert(pr->args.size()==1);
    delete pr->args[0];
    pr->args[0] = std::get<1>(correlating_predicate);

    // check if "exists" or "not exists"
    auto join = static_cast<Ra__Node__Join*>(markers_joins.second);
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
    auto marker = static_cast<Ra__Node__Where_Subquery_Marker*>(markers_joins.first);
    marker->type = newType;
}

void RaTree::get_correlating_predicates(Ra__Node* predicate, std::vector<std::tuple<Ra__Node*,Ra__Node*,std::string,size_t>>& correlating_predicates, bool& is_boolean_predicate, std::vector<std::pair<std::string,std::string>> relations_aliases){
    switch(predicate->node_case){
        case RA__NODE__BOOL_PREDICATE:{
            is_boolean_predicate = true;
            auto bool_p = static_cast<Ra__Node__Bool_Predicate*>(predicate);
                for(const auto& p: bool_p->args){
                    get_correlating_predicates(p, correlating_predicates, is_boolean_predicate, relations_aliases);
                }
            break;
        }
        case RA__NODE__PREDICATE:{
            auto p = static_cast<Ra__Node__Predicate*>(predicate);
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
void RaTree::decorrelate_exists_subquery(std::pair<Ra__Node*, Ra__Node*> markers_joins){
    // 1. check if subquery simply correlated

    // go through subquery tree, find relation aliases/names
    std::vector<std::pair<std::string,std::string>> relations_aliases;
    Ra__Node* it = (markers_joins.second)->childNodes[1]->childNodes[0];
    // start searching from child of subquery projection
    get_relations_aliases(it, relations_aliases);

    // find (first & only) selection in correlated subquery
    it = (markers_joins.second)->childNodes[1]->childNodes[0];
    // correlated exists must have selection
    assert(get_first_selection(&it));
    Ra__Node__Selection* sel = static_cast<Ra__Node__Selection*>(it);

    std::vector<std::tuple<Ra__Node*,Ra__Node*,std::string,size_t>> correlating_predicates;
    bool is_boolean_predicate = false;
    get_correlating_predicates(sel->predicate, correlating_predicates, is_boolean_predicate, relations_aliases);
    
    if(is_trivially_correlated(correlating_predicates)){
        decorrelate_exists_trivial(is_boolean_predicate, correlating_predicates[0], sel, markers_joins);
    }
    else{
        decorrelate_exists_complex(correlating_predicates, markers_joins);
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
    for(const auto& child: childNodes){
        if(!found){
            *it = child;
            found = get_join(it);
        }
    }
    return found;
}

bool get_join_parent(Ra__Node** it){
    auto childNodes = (*it)->childNodes;
    for(const auto& child: childNodes){
        if(child->node_case==RA__NODE__JOIN || child->node_case==RA__NODE__RELATION){
            return true;
        }
    }

    bool found = false;
    for(const auto& child: childNodes){
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
    for(const auto& child: childNodes){
        if(!found){
            *it = child;
            found = get_join_marker_parent(it, marker, index);
        }
    }
    return found;
}

bool RaTree::find_marker_parent(Ra__Node** it, Ra__Node__Where_Subquery_Marker* marker, int& child_index){

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
                        child_index = i;
                        return true;
                    }
                }
            }
        }
        *it = sel->predicate;
        return find_marker_parent(it, marker, child_index);
    }

    else if((*it)->node_case==RA__NODE__BOOL_PREDICATE){
        auto bool_p = static_cast<Ra__Node__Bool_Predicate*>(*it);
        for(size_t i=0; i<bool_p->args.size(); i++){
            if(bool_p->args[i]->node_case==RA__NODE__WHERE_SUBQUERY_MARKER && static_cast<Ra__Node__Where_Subquery_Marker*>(bool_p->args[i])==marker){
                child_index = i;
                return true;
            }
            // if marker is part of boolean "not", then return grandparent of marker
            else if(bool_p->args[i]->node_case==RA__NODE__BOOL_PREDICATE){
                auto bool_not_p = static_cast<Ra__Node__Bool_Predicate*>(bool_p->args[i]);
                if(bool_not_p->bool_operator==RA__BOOL_OPERATOR__NOT){
                    for(size_t j=0; j<bool_not_p->args.size(); j++){
                        if(bool_not_p->args[j]->node_case==RA__NODE__WHERE_SUBQUERY_MARKER && static_cast<Ra__Node__Where_Subquery_Marker*>(bool_not_p->args[j])==marker){
                            child_index = i;
                            return true;
                        }
                    }
                }
            }
        }
        bool found = false;
        for(const auto& arg: bool_p->args){
            if(!found){
                *it = arg;
                found = find_marker_parent(it, marker, child_index);
            }
        }
        return found;
    }

    else if((*it)->node_case==RA__NODE__PREDICATE){
        auto p = static_cast<Ra__Node__Predicate*>(*it);

        if(p->left->node_case==RA__NODE__WHERE_SUBQUERY_MARKER && static_cast<Ra__Node__Where_Subquery_Marker*>(p->left)==marker){
            child_index = 0;
            return true;
        }
        else if(p->right->node_case==RA__NODE__WHERE_SUBQUERY_MARKER && static_cast<Ra__Node__Where_Subquery_Marker*>(p->right)==marker){
            child_index = 1;
            return true;
        }
        else{
            bool found = false;

            // find in left child
            *it = p->left;
            found = find_marker_parent(it, marker, child_index);

            // find in right child
            if(!found){
                *it = p->right;
                found = find_marker_parent(it, marker, child_index);
            }

            return found;
        }
    }

    auto childNodes = (*it)->childNodes;
    bool found = false;
    for(const auto& child: childNodes){
        if(!found){
            *it = child;
            found = find_marker_parent(it, marker, child_index);
        }
    }
    return found;
}

void RaTree::decorrelate_exists_complex(std::vector<std::tuple<Ra__Node*,Ra__Node*,std::string,size_t>> correlating_predicates, std::pair<Ra__Node*, Ra__Node*> markers_joins){
    // move subquery to cte, give alias and columns (correlating attributes)
    auto exists_join = static_cast<Ra__Node__Join*>(markers_joins.second);
    auto cte_subquery = static_cast<Ra__Node__Projection*>(exists_join->childNodes[1]);
    std::string cte_name = "m" + std::to_string(counter++);
    cte_subquery->subquery_alias = cte_name;

    // select distinct correlating attributes
    cte_subquery->distinct = true;
    for(const auto& arg: cte_subquery->args){
        delete arg;
    }
    cte_subquery->args.clear();
    for(const auto& p: correlating_predicates){
        auto attr = static_cast<Ra__Node__Attribute*>(std::get<0>(p));
        cte_subquery->subquery_columns.push_back(attr->name);
        cte_subquery->args.push_back(attr);
    }

    // list all correlating relations (using tpch name check of correlated attributes)
    std::unordered_map<std::string, std::vector<Ra__Node__Attribute*>> alias_attr_map; // map relation (identifier) to correlating attributes
    for(const auto& p: correlating_predicates){
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
    for(const auto& alias_attr: alias_attr_map){
        correlated_relations.push_back(build_tpch_relation(alias_attr.first, alias_attr.second));
    }

    // add correlated relations to subquery
    Ra__Node* it = cte_subquery;
    assert(get_join_parent(&it));
    for(const auto& r: correlated_relations){
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
        for(const auto& cor_predicate: correlating_predicates){
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
    auto marker = static_cast<Ra__Node__Where_Subquery_Marker*>(markers_joins.first);
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
    
    replace_selection_marker(it, index, null_test);
}

void RaTree::replace_selection_marker(Ra__Node* marker_parent, int child_index, Ra__Node* replacement){
    switch(marker_parent->node_case){
        case RA__NODE__SELECTION:{
            auto sel = static_cast<Ra__Node__Selection*>(marker_parent);
            delete sel->predicate;
            sel->predicate = replacement;
            break;
        }
        case RA__NODE__BOOL_PREDICATE:{
            auto bool_p = static_cast<Ra__Node__Bool_Predicate*>(marker_parent);
            delete bool_p->args[child_index];
            bool_p->args[child_index] = replacement;
            break;
        }
        case RA__NODE__PREDICATE:{
            auto p = static_cast<Ra__Node__Predicate*>(marker_parent);
            if(child_index==0){
                delete p->left;
                p->left = replacement;
            }
            else if(child_index==1){
                delete p->right;
                p->right = replacement;
            }
            else{
                std::cout << "This should be unreachable :)" << std::endl;
            }
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