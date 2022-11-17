#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer &search_server)
{
    vector<set<string>> unique_docs = {};

    vector<int> docs_to_delete = {};

    for (const int doc_id : search_server)
    {
        set<string> text = {};

        for (const auto &[word, freq] :
             search_server.GetWordFrequencies(doc_id))
        {
            text.insert(word);
        }

        auto comparator = [&text](const set<string> &set_words)
        {
            return set_words == text;
        };

        if ( count_if(unique_docs.begin(), unique_docs.end(), comparator) == 0 )
        {
            unique_docs.push_back(text);
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
