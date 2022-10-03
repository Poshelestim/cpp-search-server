#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = EPSILON;

string ReadLine()
{
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber()
{
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text)
{
    vector<string> words;
    string word;
    for (const char c : text)
    {
        if (c == ' ')
        {
            if (!word.empty())
            {
                words.push_back(word);
                word.clear();
            }
        }
        else
        {
            word += c;
        }
    }
    if (!word.empty())
    {
        words.push_back(word);
    }

    return words;
}

struct Document
{
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus
{
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text)
    {
        for (const string& word : SplitIntoWords(text))
        {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
                     const vector<int>& ratings)
    {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words)
        {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id,
                           DocumentData{ComputeAverageRating(ratings), status});
    }

    vector<Document> FindTopDocuments(const string& raw_query,
                                      DocumentStatus marker = DocumentStatus::ACTUAL) const
    {
        return FindTopDocuments(raw_query,
                                [marker](int document_id,
                                DocumentStatus status,
                                int rating)
        { return status == marker; });
    }

    template<typename Marker>
    vector<Document> FindTopDocuments(const string& raw_query,
                                      Marker marker) const
    {
        const Query query = ParseQuery(raw_query);

        auto matched_documents = FindAllDocuments(query, marker);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs)
        {
            if (abs(lhs.relevance - rhs.relevance) < EPSILON)
            {
                return lhs.rating > rhs.rating;
            }
            else
            {
                return lhs.relevance > rhs.relevance;
            }
        });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT)
        {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    int GetDocumentCount() const
    {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
                                                        int document_id) const
    {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words)
        {
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id))
            {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words)
        {
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id))
            {
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

    bool IsStopWord(const string& word) const
    {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const
    {
        vector<string> words;
        for (const string& word : SplitIntoWords(text))
        {
            if (!IsStopWord(word))
            {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings)
    {
        if (ratings.empty())
        {
            return 0;
        }
        const int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord
    {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const
    {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-')
        {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query
    {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const
    {
        Query query;
        for (const string& word : SplitIntoWords(text))
        {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop)
            {
                if (query_word.is_minus)
                {
                    query.minus_words.insert(query_word.data);
                }
                else
                {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const
    {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template<typename Marker>
    vector<Document> FindAllDocuments(const Query& query, Marker marker) const
    {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words)
        {
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word))
            {
                if (marker(document_id,
                           documents_.at(document_id).status,
                           documents_.at(document_id).rating))
                {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words)
        {
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word))
            {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance)
        {
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
                     const string& func, unsigned line, const string& hint)
{
    if (t != u)
    {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty())
        {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file,
                const string& func, unsigned line, const string& hint)
{
    if (!value)
    {
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
    cerr << name_func;
    func();
    cerr << " OK" << endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func)
// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent()
{
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
    // Убеждаемся, что поиск документа, имеющего минус-слова,
    // не будет найден
    {
        SearchServer search_server;
        search_server.SetStopWords("и в на"s);
        search_server.AddDocument(11, "ухоженный белый кот и модный ошейник"s,
                                  DocumentStatus::ACTUAL, {});
        const vector<Document> result =
                search_server.FindTopDocuments("пушистый ухоженный -кот"s);
        ASSERT_HINT(result.empty(), "Найден документ с минус-словом"s);

    }

    // Убеждаемся, что поиск документа, неимеющего минус-слова,
    // будет найден
    {
        SearchServer search_server;
        search_server.SetStopWords("и в на"s);
        search_server.AddDocument(3, "ухоженный скворец евгений"s,
                                  DocumentStatus::ACTUAL, {});
        const vector<Document> result =
                search_server.FindTopDocuments("пушистый ухоженный -кот"s);
        ASSERT_HINT(!result.empty(),
                    "Не найден документ, не содержащий минус-слова"s);
    }
}

void TestMatchDocument()
{
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s,
                              DocumentStatus::ACTUAL, {});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,
                              DocumentStatus::ACTUAL, {});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,
                              DocumentStatus::BANNED, {});

    //Проверка пустого запроса
    {
        const vector<Document> result = search_server.FindTopDocuments(""s);
        ASSERT_HINT(result.empty(),
                    "Ожидается, что документов, подходящих для запроса, не существует"s);
    }
    /*----------*/

    //Проверка без минус-слов
    {
                {
                    auto [words, status] =
                            search_server.MatchDocument("пушистый кот"s, 0);

                            ASSERT_HINT(words.size() == 1,
                            "Ожидаемые слова не найдены"s);
                            ASSERT_HINT(words[0] == "кот"s,
                            "Ожидаемые слова не найдены"s);
               }

                {
                    auto [words, status] =
                            search_server.MatchDocument("пушистый кот"s, 1);

                            ASSERT_HINT(words.size() == 2,
                            "Ожидаемые слова не найдены"s);
                            ASSERT_HINT(words[1] == "пушистый"s && words[0] == "кот"s,
                            "Ожидаемые слова не найдены"s);
                }

                {
                    auto [words, status] =
                            search_server.MatchDocument("пушистый кот"s, 3);

                            ASSERT_HINT(words.empty(),
                            "Ожидается, что не будет найденных слов, соответсвующих запросу"s);
                }
    }
    /*----------*/

    //Проверка с минус-словами
        {
            auto [words, status] =
                    search_server.MatchDocument("-пушистый кот"s, 1);

            ASSERT_HINT(words.empty(),
            "Ожидаемые слова не найдены"s);
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
                              DocumentStatus::ACTUAL, {-1, -12, -4, -1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,
                              DocumentStatus::ACTUAL, {0});

    const vector<int> expected_id_document = {1, 3, 0, 2};

    const vector<Document> result =
            search_server.FindTopDocuments("пушистый ухоженный кот"s);
    ASSERT_HINT(is_sorted(result.begin(), result.end(),
                          [](const Document& lhs, const Document& rhs)
    {
        return lhs.relevance > rhs.relevance;
    }
    ), "Неотсортировано по возрастанию реливантности"s);

    vector<int> result_id_documents;
    for (const Document& doc : result)
    {
        result_id_documents.push_back(doc.id);
    }

    ASSERT_EQUAL_HINT(expected_id_document, result_id_documents,
                      "Неотсортировано по возрастанию реливантности"s);
}

void TestCalculationOfArithmeticRaiting()
{
    {
        SearchServer search_server;
        search_server.AddDocument(1, "пушистый кот пушистый хвост"s,
                                  DocumentStatus::ACTUAL,
                                  {7, 2, 7});
        const vector<Document> result =
                search_server.FindTopDocuments("пушистый ухоженный кот"s);

        ASSERT_HINT(!result.empty(),
                    "Ожидается, что документ будет найден"s);
        ASSERT_HINT((7 + 2 + 7) / 3 == result[0].rating,
                "Некорректно высчитывается рейтинг"s);
    }

    {
        SearchServer search_server;
        search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s,
                                  DocumentStatus::ACTUAL,
                                  {5, -12, 2, 1, 55, -100});
        const vector<Document> result =
                search_server.FindTopDocuments("пушистый ухоженный кот"s);

        ASSERT_HINT(!result.empty(),
                    "Ожидается, что документ будет найден"s);
        ASSERT_HINT((5 + -12 + 2 + 1 + 55 + -100) / 6 == result[0].rating,
                "Некорректно высчитывается рейтинг"s);
    }

    {
        SearchServer search_server;
        search_server.AddDocument(3, "ухоженный кот выразительные глаза"s,
                                  DocumentStatus::ACTUAL,
                                  {});
        const vector<Document> result =
                search_server.FindTopDocuments("пушистый ухоженный кот"s);

        ASSERT_HINT(!result.empty(),
                    "Ожидается, что документ будет найден"s);
        ASSERT_HINT(0 == result[0].rating,
                "Некорректно высчитывается рейтинг для добавленного документа с незаданным рейтингом"s);
    }
}

void TestFilterWithStatus()
{
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(7, "белый кот и модный ошейник"s,
                              DocumentStatus::ACTUAL, {});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,
                              DocumentStatus::BANNED, {});
    search_server.AddDocument(22, "ухоженный кот евгений"s,
                              DocumentStatus::IRRELEVANT, {});
    search_server.AddDocument(51, "ухоженный скворечник евгений"s,
                              DocumentStatus::REMOVED, {});

    {
        const vector<Document> result =
                search_server.FindTopDocuments("пушистый ухоженный кот"s,
                                               [](int document_id,
                                               DocumentStatus status,
                                               int rating)
        { return status == DocumentStatus::ACTUAL; });
        ASSERT_HINT(!result.empty(),
                    "Ожидается, что результат запроса со статусом ACTUAL не пустой"s);
        ASSERT_HINT(7 == result[0].id,
                "Документ со статусом ACTUAL не найден");

    }

    {
        const vector<Document> result =
                search_server.FindTopDocuments("пушистый ухоженный кот"s,
                                               [](int document_id,
                                               DocumentStatus status,
                                               int rating)
        { return status == DocumentStatus::BANNED; });
        ASSERT_HINT(!result.empty(),
                    "Ожидается, что результат запроса со статусом BANNED не пустой"s);
        ASSERT_HINT(3 == result[0].id,
                "Документ со статусом BANNED не найден");
    }

    {
        const vector<Document> result =
                search_server.FindTopDocuments("пушистый ухоженный кот"s,
                                               [](int document_id,
                                               DocumentStatus status,
                                               int rating)
        { return status == DocumentStatus::IRRELEVANT; });
        ASSERT_HINT(!result.empty(),
                    "Ожидается, что результат запроса со статусом IRRELEVANT не пустой"s);
        ASSERT_HINT(22 == result[0].id,
                "Документ со статусом IRRELEVANT не найден");
    }

    {
        const vector<Document> result =
                search_server.FindTopDocuments("пушистый ухоженный кот"s,
                                               [](int document_id,
                                               DocumentStatus status,
                                               int rating)
        { return status == DocumentStatus::REMOVED; });
        ASSERT_HINT(!result.empty(),
                    "Ожидается, что результат запроса со статусом REMOVED не пустой"s);
        ASSERT_HINT(51 == result[0].id,
                "Документ со статусом REMOVED не найден");
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
        ASSERT_HINT(!result.empty(), "Ожидается, что результат запроса не пустой"s);
        for (const Document& doc : result)
        {
            ASSERT_HINT(doc.id % 2 == 0,
                        "Некорректно работает поиск с учетом предиката по id. Найден документ "s +
                        to_string(doc.id) + " ошибочно подходящий под условия запроса"s);
        }
    }

    {
        const vector<Document> result =
                search_server.FindTopDocuments("пушистый ухоженный кот"s,
                                               [](int document_id,
                                               DocumentStatus status,
                                               int rating)
        { return rating > 0; });
        ASSERT_HINT(!result.empty(), "Ожидается, что результат запроса не пустой"s);
        for (const Document& doc : result)
        {
            ASSERT_HINT(doc.rating > 0,
                        "Некорректно работает поиск с учетом предиката по рейтингу. Найден документ "s +
                        to_string(doc.id) + " ошибочно подходящий под условия запроса"s);
        }
    }

    {
        const vector<Document> result =
                search_server.FindTopDocuments("пушистый ухоженный кот"s,
                                               [](int document_id,
                                               DocumentStatus status,
                                               int rating)
        { return document_id % 2 == 0 && rating > 0; });
        ASSERT_HINT(!result.empty(), "Ожидается, что результат запроса не пустой"s);
        for (const Document& doc : result)
        {
            ASSERT_HINT(doc.id % 2 == 0 && doc.rating > 0,
                        "Некорректно работает поиск с учетом предиката по рейтингу и id. Найден документ "s +
                        to_string(doc.id) + " ошибочно подходящий под условия запроса"s);
        }
    }

    {
        const vector<Document> result =
                search_server.FindTopDocuments("пушистый ухоженный кот"s,
                                               [](int document_id,
                                               DocumentStatus status,
                                               int rating)
        { return document_id < 0 && rating > 0; });
        ASSERT_HINT(!!result.empty(),
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
        "пушистый кот пушистый хвост"s,
        "ухоженный пёс выразительные глаза"s,
        "ухоженный кот евгений"s,
    };

    const double count_docs = static_cast<double>(text_docs.size());

    search_server.AddDocument(0, text_docs.at(0),
                              DocumentStatus::ACTUAL, {});
    search_server.AddDocument(1, text_docs.at(1),
                              DocumentStatus::ACTUAL, {});
    search_server.AddDocument(2, text_docs.at(2),
                              DocumentStatus::ACTUAL, {});

    map<string, map<int, double> > expected_word_relevance;

    expected_word_relevance["пушистый"][0] = log(count_docs / 1.0) * (2.0 / 4.0);
    expected_word_relevance["пушистый"][1] = log(count_docs / 1.0) * 0.0;
    expected_word_relevance["пушистый"][2] = log(count_docs / 1.0) * 0.0;

    expected_word_relevance["ухоженный"][0] = log(count_docs / 2.0) * 0.0;
    expected_word_relevance["ухоженный"][1] = log(count_docs / 2.0) * (1.0 / 4.0);
    expected_word_relevance["ухоженный"][2] = log(count_docs / 2.0) * (1.0 / 3.0);

    expected_word_relevance["кот"][0] = log(count_docs / 2.0) * (1.0 / 4.0);
    expected_word_relevance["кот"][1] = log(count_docs / 2.0) * 0.0;
    expected_word_relevance["кот"][2] = log(count_docs / 2.0) * (1.0 / 3.0);

    map<int, double> expected_relevance;
    for (size_t id_doc = 0; id_doc < text_docs.size(); ++id_doc)
    {
        expected_relevance[id_doc] = expected_word_relevance["пушистый"][id_doc] +
                expected_word_relevance["ухоженный"][id_doc] +
                expected_word_relevance["кот"][id_doc];
    }

    const vector<Document> result =
            search_server.FindTopDocuments("пушистый ухоженный кот"s);

    ASSERT_HINT(!result.empty(),
                "Ожидается, что результат запроса не пустой"s);

    map<int, double> result_relevance;
    for (const Document &doc : result)
    {
        result_relevance[doc.id] = doc.relevance;
    }

    ASSERT_EQUAL_HINT(expected_relevance, result_relevance,
                      "Некорректно рассчитывается реливантность документа"s);

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
    setlocale(LC_ALL, "Russian");
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}
