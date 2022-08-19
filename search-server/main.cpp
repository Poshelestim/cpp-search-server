#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <cmath>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
};

class SearchServer {
public:

    typedef double relevance;
    typedef int id_doc;
    typedef double ratio_in_doc;

    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document)
    {
        this->document_count_ += 1.0;
        vector<string> words = SplitIntoWordsNoStop(document);
        const double ratio = 1.0 / words.size();
        for (const string& word : words)
            word_to_document_freqs_[word][document_id] += ratio;
    }

    vector<Document> FindTopDocuments(const string& raw_query) const
    {
        Query query;
        ParseQuery(query, raw_query);
        auto matched_documents = FindAllDocuments(query);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
            return lhs.relevance > rhs.relevance;
        });

        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

private:
    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    map<string, map<id_doc, ratio_in_doc> > word_to_document_freqs_;

    set<string> stop_words_;

    double document_count_ = 0.0;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& query) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(query)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    void ParseQuery(Query& query, const string& text) const {
        for (const string& word : SplitIntoWordsNoStop(text)) {
            if (word[0] == '-')
                query.minus_words.insert(word.substr(1));
            else
                query.plus_words.insert(word);
        }
    }

    vector<Document> FindAllDocuments(const Query& query) const {
        map<id_doc, relevance> document_to_relevance;

        for (const string &word : query.plus_words)
        {
            if (!word_to_document_freqs_.count(word) )
                continue;

            const double idf =
                    log(document_count_ / word_to_document_freqs_.at(word).size() );

            for (const auto &[id_doc, ratio] : word_to_document_freqs_.at(word))
                document_to_relevance[id_doc] += idf * ratio;
        }

//        for (const auto& [word, coincidences] : word_to_document_freqs_)
//        {
//            if (query.plus_words.count(word) )
//            {
//                const double idf = log(document_count_ / coincidences.size() );
//                for (const auto& [id_doc, ratio] : coincidences)
//                {
//                    document_to_relevance[id_doc] += idf * ratio;
//                }
//            }
//        }

        for (const string &word : query.minus_words)
        {
            if (!word_to_document_freqs_.count(word) )
                continue;

            for (const auto& [id_doc, ratio] : word_to_document_freqs_.at(word))
                                document_to_relevance.erase(id_doc);
        }

//        for (const auto& [word, coincidences] : word_to_document_freqs_)
//        {
//            if (query.minus_words.count(word) )
//                for (const auto& [id_doc, ratio] : coincidences)
//                    document_to_relevance.erase(id_doc);
//        }

        vector<Document> matched_documents;
        for (const auto& [id_doc, relevance] : document_to_relevance)
            matched_documents.push_back({id_doc, relevance});

        return matched_documents;
    }

    //    static int MatchDocument(const string& content, const Query& query) {
    //        if (query.plus_words.empty()) {
    //            return 0;
    //        }
    //        set<string> matched_words;

    //        if (query.minus_words.count(content) != 0)
    //            return 0;

    //        if (query.plus_words.count(content) != 0) {
    //            matched_words.insert(content);

    //        }
    //        return static_cast<int>(matched_words.size());
    //    }
};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }

    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer();

    const string query = ReadLine();
    for (const auto& [id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << id << ", "
             << "relevance = "s << relevance << " }"s << endl;
    }
}
