/*
    Code adapted from Qiru Hu's Font.hpp
    References Qiru left for this code: 
        https://github.com/harfbuzz/harfbuzz-tutorial/blob/master/hello-harfbuzz-freetype.c 
        https://freetype.org/freetype2/docs/tutorial/step1.html
    Additional References I also looked at:
        https://learnopengl.com/code_viewer_gh.php?code=src/7.in_practice/2.text_rendering/text_rendering.cpp
 */
#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H
#include <glm/glm.hpp>

#include <hb.h>
#include <hb-ft.h>

#include <iostream>
#include <string>
#include <vector>
#include <map>

struct Text {
    std::string text;
    glm::vec2 start_pos;

    int line_length;
    int line_height;

    Text(std::string t, int ll, int lh)
    {
        text = t;
        start_pos = glm::vec2();
        line_length = ll;
        line_height = lh;
    }
    
    Text(std::string t, glm::vec2 sp, int ll, int lh)
    {
        text = t;
        start_pos = sp;
        line_length = ll;
        line_height = lh;
    }
};

struct Font {
	Font(std::string const &font_path,
        int font_size, 
        unsigned int width, 
        unsigned int height
		);
	
    virtual ~Font();

    int font_size;
    unsigned int width;
    unsigned int height;

    glm::u8vec3 color = glm::u8vec3(0);

    FT_Library ft_library;
    FT_Face ft_face;
    hb_font_t *hb_font;
    hb_buffer_t *hb_buffer;

    virtual void gen_texture(unsigned int& texture, std::vector<Text> const &texts);

    virtual std::vector<std::string> wrapText(const std::string& text, size_t line_length);
};
