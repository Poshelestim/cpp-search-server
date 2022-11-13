#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer &search_server)
{
    map<string, int> unique_docs = {};

    vector<int> docs_to_delete = {};

    for (const int doc_id : search_server)
    {
        string text = "";

        for (const auto &[word, freq] :
             search_server.GetWordFrequencies(doc_id))
        {
            text.append(word);
        }

        if (unique_docs.count(text) == 0)
        {
            unique_docs[text] += doc_id;
        }
        else
        {
            docs_to_delete.push_back(doc_id);
        }
    }

    for (const int doc_id : docs_to_delete)
    {
        cout << "Found duplicate document id "s << doc_id << endl;
        search_server.RemoveDocument(doc_id);
    }
}
