#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer &search_server)
{
    set<set<string>> unique_docs = {};

    vector<int> docs_to_delete = {};

    for (const int doc_id : search_server)
    {
        set<string> text = {};

        for (const auto &[word, freq] :
             search_server.GetWordFrequencies(doc_id))
        {
            text.insert(word);
        }

        if ( unique_docs.count(text) )
        {
            docs_to_delete.push_back(doc_id);
        }
        else
        {
            unique_docs.insert(text);
        }
    }

    for (const int doc_id : docs_to_delete)
    {
        cout << "Found duplicate document id "s << doc_id << endl;
        search_server.RemoveDocument(doc_id);
    }
}
