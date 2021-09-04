#include "common.h"
#include "Engine.h"

#include <algorithm>

#define CheckPosException(expr) {                             \
    if (!(expr))  {                                           \
        throw InvalidPositionException("Invalid position");   \
    }                                                         \
}

const int ALPHA_SIZE = 26;
Position Position::FromString(std::string_view str) {
    int row {0};
    int col {0};

    try {
        CheckPosException(std::isalpha(str.front()));
        while (std::isalpha(str.front())) {
            CheckPosException(str.front() >= 'A' && str.front() <= 'Z');
            col += col * (ALPHA_SIZE - 1) + str.front() - 'A';
            str.remove_prefix(1);
            if (std::isalpha(str.front()))
                col++;
            CheckPosException(std::isalpha(str.front()) || std::isdigit(str.front()));
        }

        CheckPosException(std::find_if(str.begin(), str.end(), [](auto ch) {
            return !isdigit(ch);
        }) == str.end());
        CheckPosException(std::isdigit(str.front()));
        CheckPosException(std::stoi(std::string{str}) > 0);
        row = std::stoi(std::string{str}) - 1;

        CheckPosException(col >= 0 && row >= 0);
        CheckPosException(col < kMaxCols && row < kMaxRows);
    } catch (...) {
        return {row, col, STATUS::ERROR};
    }

    return {row, col, STATUS::VALID};
}

std::string Position::ToString() const {
    if (!IsValid())
        return {};

    std::string index;
    int c = col;
    do {
        index += c % (ALPHA_SIZE) + 'A';
        c = c / (ALPHA_SIZE) - 1;
    } while (c >= 0);
    std::reverse(index.begin(), index.end());
    index += std::to_string(row + 1);
    return index;
}

bool Position::IsValid() const {
    if (status == STATUS::NONE) {
        status = STATUS::VALID;
        try {
            CheckPosException(col >= 0 && row >= 0);
            CheckPosException(col < kMaxCols && row < kMaxRows);
        } catch (...) {
            status = STATUS::ERROR;
        }
    }
    return status == STATUS::VALID;
}

bool Position::operator==(const Position &rhs) const {
    return row == rhs.row && col == rhs.col;
}

bool Position::operator<(const Position &rhs) const {
    return row < rhs.row || (row == rhs.row && col < rhs.col);
}

bool Size::operator==(const Size &rhs) const {
    return rows == rhs.rows && cols == rhs.cols;
}

Size::operator bool() const {
    return cols != 0 && rows != 0;
}

bool operator<(const Size &lhs, const Position &rhs) {
    return lhs.rows < rhs.row + 1 || (lhs.rows == rhs.row + 1 && lhs.cols < rhs.col + 1);
}

bool operator<(const Position &lhs, const Size &rhs) {
    return lhs.row < rhs.rows && lhs.col < rhs.cols;
}

bool operator>(const Size &lhs, const Position &rhs) {
    return rhs < lhs;
}

bool operator>(const Position &lhs, const Size &rhs) {
    return rhs < lhs;
}

bool operator==(const Size &lhs, const Position &rhs) {
    return lhs.rows == rhs.row && lhs.cols == rhs.col;
}

bool operator==(const Position &lhs, const Size &rhs) {
    return rhs == lhs;
}

FormulaError::Category FormulaError::GetCategory() const {
    return category_;
}

bool FormulaError::operator==(FormulaError rhs) const {
    return category_ == rhs.category_;
}

std::string_view FormulaError::ToString() const {
    static const std::map<Category, std::string> categories {
            {Category::Ref, "#REF!"},
            {Category::Value, "#VALUE!"},
            {Category::Div0, "#DIV/0!"}
    };
    return categories.at(category_);
}

std::ostream &operator<<(std::ostream &output, FormulaError fe) {
    output << fe.ToString();
    return output;
}

#include "Engine.h"
std::unique_ptr<ISheet> CreateSheet() {
    return std::make_unique<SpreadSheet>();
}

ProxyCell::ProxyCell(ICell *c) : cell(c), default_value(std::make_shared<DefaultCell>("")) {}
