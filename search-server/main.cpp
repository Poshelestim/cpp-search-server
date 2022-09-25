/* Подставьте вашу реализацию класса SearchServer сюда */

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
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
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
                     const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    vector<Document> FindTopDocuments(const string& raw_query,
                                      DocumentStatus marker = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query,
                                [marker](int document_id,
                                DocumentStatus status,
                                int rating)
        { return status == marker; });
    }

    template<typename Marker>
    vector<Document> FindTopDocuments(const string& raw_query,
                                      Marker marker) const {
        const Query query = ParseQuery(raw_query);

        auto matched_documents = FindAllDocuments(query, marker);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
            if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                return lhs.rating > rhs.rating;
            } else {
                return lhs.relevance > rhs.relevance;
            }
        });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
                                                        int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template<typename Marker>
    vector<Document> FindAllDocuments(const Query& query, Marker marker) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if (marker(document_id,
                           documents_.at(document_id).status,
                           documents_.at(document_id).rating))
                {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                        {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}

//int main() {
//    SearchServer search_server;
//    search_server.SetStopWords("и в на"s);

//    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
//    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
//    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
//    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

//    cout << "ACTUAL by default:"s << endl;
//    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
//        PrintDocument(document);
//    }

//    cout << "ACTUAL:"s << endl;
//    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; })) {
//        PrintDocument(document);
//    }

//    cout << "Even ids:"s << endl;
//    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
//        PrintDocument(document);
//    }

//    return 0;
//}


/*
   Подставьте сюда вашу реализацию макросов
   ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST
*/
template <typename Key, typename Value>
void Print(ostream& out, const map<Key, Value>& container)
{
    bool is_first = true;
    for (auto& [key, value] : container)
    {
        if (!is_first)
        {
            out << ", "s;
        }
        out << key << ": " << value;
        is_first = false;
    }
}

template <typename Container>
void Print(ostream& out, const Container& container)
{
    bool is_first = true;
    for (const auto& term : container)
    {
        if (!is_first)
        {
            out << ", "s;
        }
        is_first = false;
        out << term;
    }
}

template <typename Key, typename Value>
ostream& operator<<(ostream& out, const map<Key, Value>& container)
{
    out << "{"s;
    Print(out, container);
    out << "}"s;
    return out;
}

template <typename Element>
ostream& operator<<(ostream& out, const vector<Element>& container)
{
    out << "["s;
    Print(out, container);
    out << "]"s;
    return out;
}

template <typename Element>
ostream& operator<<(ostream& out, const set<Element>& container)
{
    out << "{"s;
    Print(out, container);
    out << "}"s;
    return out;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename Func>
void RunTestImpl(const Func& func, const string& name_func)
{
    /* Напишите недостающий код */
    cerr << name_func;
    func();
    cerr << " OK" << endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func)
// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        ASSERT_HINT(doc0.id == doc_id,
                    "Идентификатор документа не соответствует ожидаемому"s);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Некорректная логика работы со стоп-словами"s);
    }
}

void TestFindDocumentContentWithMinusWords()
{
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(11, "белый кот и модный ошейник"s,
                              DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(22, "пушистый кот пушистый хвост"s,
                              DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s,
                              DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,
                              DocumentStatus::ACTUAL, {9});

    {
        const vector<Document> result =
                search_server.FindTopDocuments("пушистый ухоженный -кот"s);
        ASSERT(result.size() == 2);

        bool is_searched = false;
        for (const Document& doc : result)
            if (doc.id == 2)
            {
                is_searched = true;
                break;
            }
        ASSERT_HINT(is_searched == true, "Не найден ожидаемый документ"s);

        is_searched = false;
        for (const Document& doc : result)
            if (doc.id == 3)
            {
                is_searched = true;
                break;
            }
        ASSERT_HINT(is_searched == true, "Не найден ожидаемый документ"s);
    }

    {
        const vector<Document> result =
                search_server.FindTopDocuments("скворец -кот -глаза -дерево"s);
        ASSERT_HINT(result.size() == 1, "Ожидается один найденный документ"s);
        ASSERT_HINT(result.begin()->id == 3, "Не найден ожидаемый документ"s);
    }

    {
        const vector<Document> result =
                search_server.FindTopDocuments("-пушистый -ухоженный -кот белый"s);
        ASSERT_HINT(result.size() == 0,
                    "Ожидается, что документов, подходящих для запроса, не существует"s);
    }

    {
        const vector<Document> result =
                search_server.FindTopDocuments("-пушистый -ухоженный -кот -белый"s);
        ASSERT_HINT(result.size() == 0,
                    "Ожидается, что документов, подходящих для запроса, не существует"s);
    }

    {
        const vector<Document> result =
                search_server.FindTopDocuments("пушистый ухоженный кот -дерево"s);
        ASSERT_HINT(result.size() == 4,
                    "Ожидается, что все добавленные документы будут являтся результатом запроса"s);
    }
}

void TestMatchDocument()
{
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s,
                              DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,
                              DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s,
                              DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,
                              DocumentStatus::BANNED, {9});

    //Проверка пустого запроса
    {
        const vector<Document> result = search_server.FindTopDocuments(""s);
        ASSERT_HINT(!result.size(),
                    "Ожидается, что документов, подходящих для запроса, не существует"s);
    }
    /*----------*/

    //Проверка без минус-слов
    {
        const int doc_count = search_server.GetDocumentCount();
        for (int document_id = 0; document_id < doc_count; ++document_id)
        {
            const auto result =
                    search_server.MatchDocument("пушистый кот"s, document_id);
            if (document_id == 0)
            {
                ASSERT_HINT(get<0>(result).size() == 1,
                            "Ожидается, что будет найдено одно слово"s);
                ASSERT_HINT(get<0>(result)[0] == "кот"s,
                        "Ожидается, что найденное слово является \"кот\""s);
                continue;
            }
            else if (document_id == 1)
            {
                bool is_searched = false;
                for (const string& word : get<0>(result))
                    if (word == "пушистый"s)
                    {
                        is_searched = true;
                        break;
                    }
                ASSERT_HINT(is_searched == true,
                            "Ожидается, что одним из найденных слов является \"пушистый\""s);

                is_searched = false;
                for (const string& word : get<0>(result))
                    if (word == "кот"s)
                    {
                        is_searched = true;
                        break;
                    }
                ASSERT_HINT(is_searched == true,
                            "Ожидается, что одним из найденных слов является \"кот\""s);
                continue;
            }
            ASSERT_HINT(!get<0>(result).size(),
                        "Ожидается, что не будет найденных слов, соответсвующих запросу"s);
        }
    }

    {
        const auto result =
                search_server.MatchDocument("пушистый собака"s, 1);
        ASSERT_HINT(get<0>(result).size() == 1,
               "Ожидается, что будет найден один документ, соответсвующий запросу"s);

        bool is_searched = false;
        for (const string& word : get<0>(result))
            if (word == "пушистый"s)
            {
                is_searched = true;
                break;
            }
        ASSERT_HINT(is_searched == true,
                    "Ожидается, что одним из найденных слов является \"пушистый\""s);
    }

    {
        const int doc_count = search_server.GetDocumentCount();
        for (int document_id = 0; document_id < doc_count; ++document_id)
        {
            const auto result =
                    search_server.MatchDocument("собака"s, document_id);
            ASSERT_HINT(!get<0>(result).size(),
                        "Ожидается, что не будет найденных слов, соответсвующих запросу"s);
        }
    }
    /*----------*/

    //Проверка с минус-словами
    {
        const int doc_count = search_server.GetDocumentCount();
        for (int document_id = 0; document_id < doc_count; ++document_id)
        {
            const auto result =
                    search_server.MatchDocument("-пушистый кот"s, document_id);
            if (document_id == 0)
            {
                ASSERT_HINT(get<0>(result).size() == 1,
                            "Ожидается, что будет найдено одно слово"s);
                ASSERT_HINT(get<0>(result)[0] == "кот"s,
                         "Ожидается, что найденным словом является \"кот\""s);
                continue;
            }
            ASSERT_HINT(!get<0>(result).size(),
                        "Ожидается, что не будет найденных слов, соответсвующих запросу"s);
        }
    }

    {
        const int doc_count = search_server.GetDocumentCount();
        for (int document_id = 0; document_id < doc_count; ++document_id)
        {
            const auto result =
                    search_server.MatchDocument("пушистый -кот"s, document_id);
            ASSERT_HINT(!get<0>(result).size(),
                        "Ожидается, что не будет найденных слов, соответсвующих запросу"s);
        }
    }

    {
        const int doc_count = search_server.GetDocumentCount();
        for (int document_id = 0; document_id < doc_count; ++document_id)
        {
            const auto result =
                    search_server.MatchDocument("пушистый кот -скворец"s, document_id);
            if (document_id == 0)
            {
                ASSERT_HINT(get<0>(result).size() == 1,
                            "Ожидается, что будет найдено одно слово"s);
                ASSERT_HINT(get<0>(result)[0] == "кот"s,
                         "Ожидается, что найденным словом является \"кот\""s);
                continue;
            }

            if (document_id == 1)
            {
                bool is_searched = false;
                for (const string& word : get<0>(result))
                    if (word == "пушистый"s)
                    {
                        is_searched = true;
                        break;
                    }
                ASSERT_HINT(is_searched == true,
                            "Ожидается, что одним из найденных слов является \"пушистый\""s);

                is_searched = false;
                for (const string& word : get<0>(result))
                    if (word == "кот"s)
                    {
                        is_searched = true;
                        break;
                    }
                ASSERT_HINT(is_searched == true,
                            "Ожидается, что одним из найденных слов является \"кот\""s);
                continue;
            }

            ASSERT_HINT(!get<0>(result).size(),
                        "Ожидается, что не будет найденных слов, соответсвующих запросу"s);
        }
    }
    /*----------*/
}

void TestResultSortedByRelevanceInDescendingOrder()
{
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s,
                              DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,
                              DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s,
                              DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,
                              DocumentStatus::BANNED, {9});

    const vector<Document> result =
            search_server.FindTopDocuments("пушистый ухоженный кот"s);
    ASSERT_HINT(is_sorted(result.begin(), result.end(),
                          [](const Document& lhs, const Document& rhs)
    {
        return lhs.relevance > rhs.relevance;
    }
    ), "Неотсортировано по возрастанию реливантности"s)  ;
}

void TestCalculationOfArithmeticRaiting()
{
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    const vector <int> rating1 = {7, 2, 7};
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,
                              DocumentStatus::ACTUAL, rating1);
    const vector <int> rating2 = {5, -12, 2, 1, 55, -100};
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s,
                              DocumentStatus::ACTUAL, rating2);
    const vector <int> rating3 = {0};
    search_server.AddDocument(3, "ухоженный кот выразительные глаза"s,
                              DocumentStatus::ACTUAL, rating3);
    const vector <int> rating4 = {-5, -12, -2, -1, -55, -100};
    search_server.AddDocument(4, "ухоженный пёс выразительная шерсть"s,
                              DocumentStatus::ACTUAL, rating4);

    map<int, vector<int> > id_and_rating
    {
        {1, rating1}, {2, rating2}, {3, rating3}, {4, rating4}
    };

    map<int, int> expected_id_and_rating;
    for (const auto& rating : id_and_rating)
    {
        for (const int& value : rating.second)
            expected_id_and_rating[rating.first] += value;
        expected_id_and_rating[rating.first] /= static_cast<int>(rating.second.size());
    }

    const vector<Document> result =
            search_server.FindTopDocuments("пушистый ухоженный кот"s);

    map<int, int> result_id_and_calc_rating;
    for (const Document& doc : result)
        result_id_and_calc_rating[doc.id] += doc.rating;

    ASSERT_EQUAL_HINT(expected_id_and_rating, result_id_and_calc_rating,
                      "Некорректно рассчитывается рейтинг"s);
}

void TestFilterWithStatus()
{
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s,
                              DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,
                              DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s,
                              DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,
                              DocumentStatus::BANNED, {9});
    search_server.AddDocument(4, "ухоженный кот евгений"s,
                              DocumentStatus::IRRELEVANT, {9, -10});
    search_server.AddDocument(5, "ухоженный скворечник евгений"s,
                              DocumentStatus::REMOVED, {9, 9, 9, 9, 9});

    map<int, DocumentStatus > id_and_status
    {
        {0, DocumentStatus::ACTUAL},
        {1, DocumentStatus::ACTUAL},
        {2, DocumentStatus::ACTUAL},
        {3, DocumentStatus::BANNED},
        {4, DocumentStatus::IRRELEVANT},
        {5, DocumentStatus::REMOVED},
    };

    {
        const vector<Document> result =
                search_server.FindTopDocuments("пушистый ухоженный кот"s);
        ASSERT_HINT(result.size(), "Ожидается, что результат запроса не пустой"s);
        for (const Document& doc : result)
            ASSERT_HINT(id_and_status.at(doc.id) == DocumentStatus::ACTUAL,
                        "Не совпадает с ожидаемым статусом в документе "s + to_string(doc.id));
    }

    {
        const vector<Document> result =
                search_server.FindTopDocuments("пушистый ухоженный кот"s,
                                               [](int document_id,
                                               DocumentStatus status,
                                               int rating)
        { return status == DocumentStatus::ACTUAL; });
        ASSERT_HINT(result.size(), "Ожидается, что результат запроса не пустой"s);
        for (const Document& doc : result)
            ASSERT_HINT(id_and_status.at(doc.id) == DocumentStatus::ACTUAL,
                        "Не совпадает с ожидаемым статусом в документе "s + to_string(doc.id));
    }

    {
        const vector<Document> result =
                search_server.FindTopDocuments("пушистый ухоженный кот"s,
                                               [](int document_id,
                                               DocumentStatus status,
                                               int rating)
        { return status == DocumentStatus::BANNED; });
        ASSERT_HINT(result.size(), "Ожидается, что результат запроса не пустой"s);
        for (const Document& doc : result)
            ASSERT_HINT(id_and_status.at(doc.id) == DocumentStatus::BANNED,
                        "Не совпадает с ожидаемым статусом в документе "s + to_string(doc.id));
    }

    {
        const vector<Document> result =
                search_server.FindTopDocuments("пушистый ухоженный кот"s,
                                               [](int document_id,
                                               DocumentStatus status,
                                               int rating)
        { return status == DocumentStatus::IRRELEVANT; });
        ASSERT_HINT(result.size(), "Ожидается, что результат запроса не пустой"s);
        for (const Document& doc : result)
            ASSERT_HINT(id_and_status.at(doc.id) == DocumentStatus::IRRELEVANT,
                        "Не совпадает с ожидаемым статусом в документе "s + to_string(doc.id));
    }

    {
        const vector<Document> result =
                search_server.FindTopDocuments("пушистый ухоженный кот"s,
                                               [](int document_id,
                                               DocumentStatus status,
                                               int rating)
        { return status == DocumentStatus::REMOVED; });
        ASSERT_HINT(result.size(), "Ожидается, что результат запроса не пустой"s);
        for (const Document& doc : result)
            ASSERT_HINT(id_and_status.at(doc.id) == DocumentStatus::REMOVED,
                        "Не совпадает с ожидаемым статусом в документе "s + to_string(doc.id));
    }

}

void TestFilterWithPredicate()
{
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s,
                              DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,
                              DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s,
                              DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,
                              DocumentStatus::ACTUAL, {9});
    search_server.AddDocument(4, "ухоженный кот евгений"s,
                              DocumentStatus::ACTUAL, {9, -10});
    search_server.AddDocument(5, "ухоженный скворечник евгений"s,
                              DocumentStatus::ACTUAL, {9, 9, -9, 9, 9});

    {
        const vector<Document> result =
                search_server.FindTopDocuments("пушистый ухоженный кот"s,
                                               [](int document_id,
                                               DocumentStatus status,
                                               int rating)
        { return document_id % 2 == 0; });
        ASSERT_HINT(result.size(), "Ожидается, что результат запроса не пустой"s);
        for (const Document& doc : result)
            ASSERT_HINT(doc.id % 2 == 0,
                        "Некорректно работает поиск с учетом предиката по id. Найден документ "s +
                        to_string(doc.id) + " ошибочно подходящий под условия запроса"s);
    }

    {
        const vector<Document> result =
                search_server.FindTopDocuments("пушистый ухоженный кот"s,
                                               [](int document_id,
                                               DocumentStatus status,
                                               int rating)
        { return rating > 0; });
        ASSERT_HINT(result.size(), "Ожидается, что результат запроса не пустой"s);
        for (const Document& doc : result)
            ASSERT_HINT(doc.rating > 0,
                        "Некорректно работает поиск с учетом предиката по рейтингу. Найден документ "s +
                        to_string(doc.id) + " ошибочно подходящий под условия запроса"s);
    }

    {
        const vector<Document> result =
                search_server.FindTopDocuments("пушистый ухоженный кот"s,
                                               [](int document_id,
                                               DocumentStatus status,
                                               int rating)
        { return document_id % 2 == 0 && rating > 0; });
        ASSERT_HINT(result.size(), "Ожидается, что результат запроса не пустой"s);
        for (const Document& doc : result)
            ASSERT_HINT(doc.id % 2 == 0 && doc.rating > 0,
                        "Некорректно работает поиск с учетом предиката по рейтингу и id. Найден документ "s +
                        to_string(doc.id) + " ошибочно подходящий под условия запроса"s);
    }

    {
        const vector<Document> result =
                search_server.FindTopDocuments("пушистый ухоженный кот"s,
                                               [](int document_id,
                                               DocumentStatus status,
                                               int rating)
        { return document_id < 0 && rating > 0; });
        ASSERT_HINT(!result.size(),
                    "Некорректно работает поиск с учетом предиката по рейтингу и id. "s
                    "Количество найденных документов: "s +
                    to_string(result.size()) +
                    ". Ожидается, что документов, подходящих для запроса, не будут найдены"s);
    }
}

void TestCalculationRelevance()
{
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    const vector<string> text_docs =
    {
        "белый кот и модный ошейник"s,
        "пушистый кот пушистый хвост"s,
        "ухоженный пёс выразительные глаза"s,
        "ухоженный скворец евгений"s,
        "ухоженный кот евгений"s,
        "пушистый скворечник"s,
        "пес в будке"s,
    };

    const string query = "пушистый ухоженный кот"s;

    map<int, double> expected_relevances
    {
//        {0, 0.211824465097},
        {1, 0.838205949344},
        {2, 0.211824465097},
        {3, 0.282432620129},
        {4, 0.564865240258},
        {5, 0.626381484248},
//        {6, 0.0},
    };

    search_server.AddDocument(0, text_docs.at(0),
                              DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, text_docs.at(1),
                              DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, text_docs.at(2),
                              DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, text_docs.at(3),
                              DocumentStatus::ACTUAL, {9});
    search_server.AddDocument(4, text_docs.at(4),
                              DocumentStatus::ACTUAL, {9, -10});
    search_server.AddDocument(5, text_docs.at(5),
                              DocumentStatus::ACTUAL, {9, 9, -9, 9, 9});
    search_server.AddDocument(6, text_docs.at(6),
                              DocumentStatus::ACTUAL, {9, 9, -9, 9, 9});


    const vector<Document> result =
            search_server.FindTopDocuments(query);
    map<int, double> result_relevances;
    for (const Document& doc : result)
        result_relevances[doc.id] += doc.relevance;

    ASSERT_EQUAL_HINT(expected_relevances, result_relevances,
                      "Некорректно рассчитывается реливантность"s);
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestFindDocumentContentWithMinusWords);
    RUN_TEST(TestMatchDocument);
    RUN_TEST(TestResultSortedByRelevanceInDescendingOrder);
    RUN_TEST(TestCalculationOfArithmeticRaiting);
    RUN_TEST(TestFilterWithPredicate);
    RUN_TEST(TestFilterWithStatus);
    RUN_TEST(TestCalculationRelevance);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}
