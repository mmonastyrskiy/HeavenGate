/*
 * Filename: d:\HeavenGate\include\colorText.h
 * Path: d:\HeavenGate\include
 * Created Date: Saturday, November 8th 2025, 11:03:47 pm
 * Author: mmonastyrskiy
 * 
 * Copyright (c) 2025 Your Company
 */

#ifndef COLOR_TEXT_H
#define COLOR_TEXT_H

#include <iostream>
#include <string>
#include <sstream>

namespace color {

// Foreground colors
enum class Color {
    BLACK = 30, RED = 31, GREEN = 32, YELLOW = 33, BLUE = 34, 
    MAGENTA = 35, CYAN = 36, WHITE = 37, DEFAULT = 39,
    BRIGHT_BLACK = 90, BRIGHT_RED = 91, BRIGHT_GREEN = 92, 
    BRIGHT_YELLOW = 93, BRIGHT_BLUE = 94, BRIGHT_MAGENTA = 95, 
    BRIGHT_CYAN = 96, BRIGHT_WHITE = 97
};

class TextBuilder {
private:
    std::string content;

public:
    TextBuilder() = default;
    
    // Явно разрешаем перемещение
    TextBuilder(const TextBuilder&) = default;
    TextBuilder& operator=(TextBuilder&&) = default;
    // Запрещаем копирование
    TextBuilder(const TextBuilder&) = delete;

    // Основной метод для добавления текста
    template<typename T>
    TextBuilder& operator<<(const T& value) {
        std::stringstream temp;
        temp << value;
        content += temp.str();
        return *this;
    }

    // Короткие алиасы для цветов
    TextBuilder& red() { return set_color(Color::RED); }
    TextBuilder& green() { return set_color(Color::GREEN); }
    TextBuilder& blue() { return set_color(Color::BLUE); }
    TextBuilder& yellow() { return set_color(Color::YELLOW); }
    TextBuilder& magenta() { return set_color(Color::MAGENTA); }
    TextBuilder& cyan() { return set_color(Color::CYAN); }
    TextBuilder& white() { return set_color(Color::WHITE); }
    TextBuilder& black() { return set_color(Color::BLACK); }

    // Яркие цвета
    TextBuilder& bright_red() { return set_color(Color::BRIGHT_RED); }
    TextBuilder& bright_green() { return set_color(Color::BRIGHT_GREEN); }
    TextBuilder& bright_blue() { return set_color(Color::BRIGHT_BLUE); }
    TextBuilder& bright_yellow() { return set_color(Color::BRIGHT_YELLOW); }
    TextBuilder& bright_magenta() { return set_color(Color::BRIGHT_MAGENTA); }
    TextBuilder& bright_cyan() { return set_color(Color::BRIGHT_CYAN); }
    TextBuilder& bright_white() { return set_color(Color::BRIGHT_WHITE); }
    TextBuilder& bright_black() { return set_color(Color::BRIGHT_BLACK); }

    // Стили
    TextBuilder& bold() { return set_style(1); }
    TextBuilder& italic() { return set_style(3); }
    TextBuilder& underline() { return set_style(4); }

    // Вывод
    void print() const {
        std::cout << content << "\033[0m" << std::flush;
    }

    void println() const {
        std::cout << content << "\033[0m" << std::endl;
    }

    std::string str() const {
        return content + "\033[0m";
    }

private:
    TextBuilder& set_color(Color c) {
        content += "\033[" + std::to_string(static_cast<int>(c)) + "m";
        return *this;
    }

    TextBuilder& set_style(int style_code) {
        content += "\033[" + std::to_string(style_code) + "m";
        return *this;
    }
};

// Статические функции - возвращают по значению (с перемещением)
class print {
public:
    static TextBuilder red()   { return TextBuilder().red(); }
    static TextBuilder green() { return TextBuilder().green(); }
    static TextBuilder blue()  { return TextBuilder().blue(); }
    static TextBuilder yellow(){ return TextBuilder().yellow(); }
    static TextBuilder magenta(){ return TextBuilder().magenta(); }
    static TextBuilder cyan()  { return TextBuilder().cyan(); }
    static TextBuilder white() { return TextBuilder().white(); }
    
    static TextBuilder bright_red()   { return TextBuilder().bright_red(); }
    static TextBuilder bright_green() { return TextBuilder().bright_green(); }
    static TextBuilder bright_blue()  { return TextBuilder().bright_blue(); }
    static TextBuilder bright_yellow(){ return TextBuilder().bright_yellow(); }

    // Готовые стили
    static TextBuilder error() { 
        return TextBuilder().bright_red().bold();
    }
    
    static TextBuilder success() { 
        return TextBuilder().bright_green().bold();
    }
    
    static TextBuilder warning() { 
        return TextBuilder().bright_yellow().bold();
    }
    
    static TextBuilder info() { 
        return TextBuilder().bright_blue();
    }

    // Пустой builder
    static TextBuilder text() { 
        return TextBuilder(); 
    }
    static void println(){
        TextBuilder().println();
    }
};

} // namespace color

#endif // COLOR_TEXT_H

/*   // Просто и элегантно!
    print::red() << "Красный текст" << print::println();
    print::green() << "Зеленый текст" << print::println();
    
    // Готовые стили
    print::error() << "Ошибка загрузки!" << print::println();
    print::success() << "Файл сохранен!" << print::println();
    print::warning() << "Внимание: низкая память" << print::println();
    print::info() << "Пользователь вошел" << print::println();
    
    // Комбинирование в одной строке
    print::red() << "Ошибка в " << print::yellow() << "файле config.txt" << print::println();
    
    // Числа и переменные
    int line = 42;
    print::error() << "Синтаксическая ошибка на строке " << line << print::println();
    
    // Цепочка стилей
    print::bright_red().bold().underline() << "ВАЖНОЕ СООБЩЕНИЕ" << print::println();
    
    return 0;
}*/