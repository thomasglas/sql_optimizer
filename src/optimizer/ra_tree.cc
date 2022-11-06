#include "ra_tree.h"

RaTree::RaTree(Ra__Node* _root, std::vector<Ra__Node*> _ctes)
:root(_root), ctes(_ctes){
}

RaTree::~RaTree(){
    delete root;
    for(auto cte: ctes){
        delete cte;
    }
};



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