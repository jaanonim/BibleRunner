const fs = require("fs");
const path = require("path");

const booksFile = fs.readFileSync("./books/books.json", "utf-8");
let books = JSON.parse(booksFile);

const directoryPath = "./books";

fs.readdirSync(directoryPath).forEach((file) => {
    const filePath = path.join(directoryPath, file);
    const jsonData = fs.readFileSync(filePath, "utf-8");

    const data = JSON.parse(jsonData);
    for (const key in data) {
        books[key].push(...data[key]);
    }
});

const reversedBooks = {};
for (const key in books) {
    for (const value of books[key]) {
        reversedBooks[value] = key;
    }
}

console.log(reversedBooks);

let cppMapString = "std::map<std::string, std::string> books = {\n";

for (const key in reversedBooks) {
    cppMapString += `  {"${key}", "${reversedBooks[key]}"},\n`;
}
cppMapString += "};";
fs.writeFileSync("books.h", cppMapString, "utf-8");
