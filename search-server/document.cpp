#include "document.h"

std::ostream& operator<<(std::ostream& os, const Document& doc)
{
    using namespace std::literals::string_literals;
    os << "{ document_id = "s << doc.id
       << ", relevance = " << doc.relevance
       << ", rating = " << doc.rating
       << " }";
    return os;
}
