#include "tests.h"

#include <iostream>
#include <cmath>

#include "search_server.h"
#include "request_queue.h"
#include "paginator.h"

using namespace std;

template <typename Key, typename Value>
void Print(std::ostream& out, const map<Key, Value>& container)
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
void Print(std::ostream& out, const Container& container)
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
std::ostream& operator<<(std::ostream& out, const map<Key, Value>& container)
{
    out << "{"s;
    Print(out, container);
    out << "}"s;
    return out;
}

template <typename Element>
std::ostream& operator<<(std::ostream& out, const vector<Element>& container)
{
    out << "["s;
    Print(out, container);
    out << "]"s;
    return out;
}

template <typename Element>
std::ostream& operator<<(std::ostream& out, const set<Element>& container)
{
    out << "{"s;
    Print(out, container);
    out << "}"s;
    return out;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t,
                     const U& u,
                     const std::string& t_str,
                     const std::string& u_str,
                     const std::string& file,
                     const std::string& func,
                     unsigned line,
                     const std::string& hint)
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

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, \
    __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, \
    __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value,
                const std::string& expr_str,
                const std::string& file,
                const std::string& func,
                unsigned line,
                const std::string& hint)
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

#define ASSERT(expr) AssertImpl(!!(expr), #expr, \
    __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, \
    __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename Func>
void RunTestImpl(const Func& func, const std::string& name_func)
{
    cerr << name_func;
    func();
    cerr << " OK" << endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func)
// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает
// стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent()
{
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server({});
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
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Некорректная логика работы со стоп-словами"s);
    }
    {
        vector<string> stop_words = {"in"s, "the"s};
        SearchServer server(stop_words);
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
        SearchServer search_server("и в на"s);
        search_server.AddDocument(11, "ухоженный белый кот и модный ошейник"s,
                                  DocumentStatus::ACTUAL, {});
        const vector<Document> result =
                search_server.FindTopDocuments("пушистый ухоженный -кот"s);
        ASSERT_HINT(result.empty(), "Найден документ с минус-словом"s);

    }

    // Убеждаемся, что поиск документа, неимеющего минус-слова,
    // будет найден
    {
        SearchServer search_server("и в на"s);
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
    SearchServer search_server("и в на"s);
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
                    "Ожидается, что документов, "
                    "подходящих для запроса, не существует"s);
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
                        "Ожидается, что не будет найденных "
                        "слов, соответсвующих запросу"s);
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
    SearchServer search_server("и в на"s);
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
        SearchServer search_server(""s);
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
        SearchServer search_server(""s);
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
        SearchServer search_server(""s);
        search_server.AddDocument(3, "ухоженный кот выразительные глаза"s,
                                  DocumentStatus::ACTUAL,
                                  {});
        const vector<Document> result =
                search_server.FindTopDocuments("пушистый ухоженный кот"s);

        ASSERT_HINT(!result.empty(),
                    "Ожидается, что документ будет найден"s);
        ASSERT_HINT(0 == result[0].rating,
                "Некорректно высчитывается рейтинг для "
                "добавленного документа с незаданным рейтингом"s);
    }
}

void TestFilterWithStatus()
{
    SearchServer search_server("и в на"s);

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
                    "Ожидается, что результат запроса "
                    "со статусом ACTUAL не пустой"s);
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
                    "Ожидается, что результат запроса "
                    "со статусом BANNED не пустой"s);
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
                    "Ожидается, что результат запроса "
                    "со статусом IRRELEVANT не пустой"s);
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
                    "Ожидается, что результат запроса "
                    "со статусом REMOVED не пустой"s);
        ASSERT_HINT(51 == result[0].id,
                "Документ со статусом REMOVED не найден");
    }

}

void TestFilterWithPredicate()
{
    SearchServer search_server("и в на"s);

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
                        "Некорректно работает поиск с учетом "
                        "предиката по рейтингу и id. Найден документ "s +
                        to_string(doc.id) +
                        " ошибочно подходящий под условия запроса"s);
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
                    "Некорректно работает поиск с "
                    "учетом предиката по рейтингу и id. "s
                    "Количество найденных документов: "s +
                    to_string(result.size()) +
                    ". Ожидается, что документов, "
                    "подходящих для запроса, не будут найдены"s);
    }
}

void TestCalculationRelevance()
{
    SearchServer search_server("и в на"s);

    const vector<std::string> text_docs =
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

    map<std::string, map<int, double> > expected_word_relevance;

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

void TestPaginator()
{
    SearchServer search_server("and with"s);
    search_server.AddDocument(1, "funny pet and nasty rat"s,
                              DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "funny pet with curly hair"s,
                              DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big cat nasty hair"s,
                              DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog cat Vladislav"s,
                              DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "big dog hamster Borya"s,
                              DocumentStatus::ACTUAL, {1, 1, 1});
    const auto search_results = search_server.FindTopDocuments("curly dog"s);
    size_t page_size = 2;
    const auto pages = Paginate(search_results, page_size);

    ASSERT_HINT(pages.size() == 2,
                "Неверно формируются страницы"s);
    ASSERT_HINT( (pages.begin()->begin()->id == 2) &&
                 ( (pages.begin()->begin() + 1)->id == 4) &&
                 ( (pages.begin() + 1)->begin()->id == 5),
                "Неверно формируются страницы"s);
}

void TestRequestQueue()
{
    SearchServer search_server("and in at"s);
    RequestQueue request_queue(search_server);
    search_server.AddDocument(1, "curly cat curly tail"s,
                              DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "curly dog and fancy collar"s,
                              DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big cat fancy collar "s,
                              DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog sparrow Eugene"s,
                              DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "big dog sparrow Vasiliy"s,
                              DocumentStatus::ACTUAL, {1, 1, 1});
    // 1439 запросов с нулевым результатом
    for (int ii = 0; ii < 1439; ++ii)
    {
        request_queue.AddFindRequest("empty request"s);
    }

    ASSERT_HINT(request_queue.GetNoResultRequests() == 1439,
                "Некорректно считаются поисковые запросы"s);

    // все еще 1439 запросов с нулевым результатом
    request_queue.AddFindRequest("curly dog"s);
    // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
    request_queue.AddFindRequest("big collar"s);
    // первый запрос удален, 1437 запросов с нулевым результатом
    request_queue.AddFindRequest("sparrow"s);
    ASSERT_HINT(request_queue.GetNoResultRequests() == 1437,
                "Некорректно считаются поисковые запросы "
                "после наступления нового дня"s);
}
// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer()
{
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestFindDocumentContentWithMinusWords);
    RUN_TEST(TestMatchDocument);
    RUN_TEST(TestResultSortedByRelevanceInDescendingOrder);
    RUN_TEST(TestCalculationOfArithmeticRaiting);
    RUN_TEST(TestFilterWithPredicate);
    RUN_TEST(TestFilterWithStatus);
    RUN_TEST(TestCalculationRelevance);
    RUN_TEST(TestPaginator);
    RUN_TEST(TestRequestQueue);
}

// --------- Окончание модульных тестов поисковой системы -----------
