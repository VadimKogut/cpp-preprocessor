#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

static const regex include_local(R"(\s*#\s*include\s*\"([^\"]*)\")");
static const regex include_global(R"(\s*#\s*include\s*<([^>]*)>)");


// напишите эту функцию
bool Preprocess(const path& current_file, const path& out_file, const vector<path>& include_directories, ostream* output = nullptr, int depth = 0) {
    
    if (depth == 0) {
        // Открываем основной входной файл
        ifstream input(current_file);
        if (!input.is_open()) {
            return false;
        }

        // Открываем выходной файл
        ofstream output_stream(out_file);
        if (!output_stream.is_open()) {
            return false;
        }

        // Запускаем обработку
        return Preprocess(current_file, out_file, include_directories, &output_stream, depth + 1);
    }

    // Обработка вложенных файлов
    ifstream input(current_file);
    if (!input.is_open()) {
        cout << "unknown include file " << current_file << " at file " << current_file
            << " at line 0\n";
        return false;
    }

    ostream& out = *output;
    string line;
    int line_number = 0;

    while (getline(input, line)) {
        line_number++;
        smatch match;

        // Если строка содержит директиву #include
        if (regex_match(line, match, include_local) || regex_match(line, match, include_global)) {
            path include_file = string(match[1]);
            path include_path;

            // Проверяем локальные файлы
            if (regex_match(line, match, include_local)) {
                include_path = current_file.parent_path() / include_file;
                if (!ifstream(include_path).is_open()) {
                    include_path.clear();
                }
            }

            // Если локальный файл не найден или это глобальный include
            if (include_path.empty()) {
                for (const auto& dir : include_directories) {
                    path candidate = dir / include_file;
                    if (ifstream(candidate).is_open()) {
                        include_path = candidate;
                        break;
                    }
                }
            }

            // Если файл так и не найден
            if (include_path.empty()) {
                std::cout << "unknown include file " << include_file.string() << " at file " 
                    << current_file.string() << " at line " << line_number << std::endl;
                return false;
            }

            // Рекурсивно обрабатываем найденный файл
            if (!Preprocess(include_path, out_file, include_directories, output, depth + 1)) {
                return false;
            }
        }
        else {
            // Записываем строку в выходной поток
            out << line << '\n';
        }
    }

    return true;
}

string GetFileContents(string file) {
    ifstream stream(file);
    // конструируем string по двум итераторам
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}
