# cpp-search-server
Поисковая система, которая ищет документы по ключевым словам и ранжирует их.

## Описание
Это приложение поисковой системы выдает упорядоченный список документов по ключевым словам по хранящимся документам.
Примеры использования находятся в [tests.cpp](https://github.com/Poshelestim/cpp-search-server/blob/main/search-server/tests.cpp).

### Функционал
 * Добавление документов в виде строки;
 * Стоп-слова при добавлении документов исключают слова для учета поисковой системой; 
 * Удаление дубликатов при добавлении документов;
 * Минус-слова исключают из результатов поиска документы;
 * Для документов формируется средняя оценка пользователей;
 * Результаты поиска выдаются по статистической мере TF-IDF;
 * Выдает максимум пять найденных документов при ответе на запрос;
 * Результаты поиска можно выдать в виде "страниц";
 * Результаты поиска можно выдать с учетом фильтра по статусу документа (Актуальный, Неактуальный, Заблокированный, Был удален), средней оценке и идентификтору документа;
 * Cоздание и обработка очереди запросов;
 * Возможность работы в многопоточном режиме;

## Сборка
Сборка с помощью любой IDE либо сборка из командной строки.

## Системные требования
Компилятор С++ с поддержкой стандарта C++17 или новее.
