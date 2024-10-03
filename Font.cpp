/*
    Code adapted from Qiru Hu's Font.cpp
    References Qiru left for this code: 
        https://github.com/harfbuzz/harfbuzz-tutorial/blob/master/hello-harfbuzz-freetype.c 
        https://freetype.org/freetype2/docs/tutorial/step1.html
    Additional References I looked at:
        https://learnopengl.com/code_viewer_gh.php?code=src/7.in_practice/2.text_rendering/text_rendering.cpp
 */

#include "Font.hpp"
#include "gl_errors.hpp"

Font::Font(std::string const &font_path, int font_size, unsigned int width, unsigned int height) {
    this->font_size = font_size;
    this->width = width;
    this->height = height;

    const char *fontfile = font_path.c_str();
    FT_Error ft_error;

    /* Initialize FreeType and create FreeType font face. */
    if ((ft_error = FT_Init_FreeType (&ft_library))) {
        std::cerr << "Error initializing FreeType library" << std::endl;
        abort();
    }
    if ((ft_error = FT_New_Face (ft_library, fontfile, 0, &ft_face))) {
        std::cerr << "Error loading font file: " << font_path << std::endl;
        abort();
    }
    if ((ft_error = FT_Set_Char_Size (ft_face, font_size*64, font_size*64, 0, 0))) {
        std::cerr << "Error setting character size" << std::endl;
        abort();
    }

    /* Create hb-ft font. */
    hb_font = hb_ft_font_create (ft_face, NULL);
    hb_buffer = hb_buffer_create();
}

Font::~Font() {
    hb_font_destroy(hb_font);
    hb_buffer_destroy(hb_buffer);
    FT_Done_Face(ft_face);
    FT_Done_FreeType(ft_library);
}

void Font::gen_texture(unsigned int& texture, std::vector<Text> const &texts) {
    /* Initialize image. Origin is the upper left corner */
    glm::u8vec4 image[height][width];
    for (int i = 0; i < height; i++ ){
        for (int j = 0; j < width; j++ )
            image[i][j] = glm::u8vec4();
    }
    FT_Error ft_error;
    FT_GlyphSlot slot = ft_face->glyph;

    auto draw_bitmap = [&image, this](FT_Bitmap*  bitmap, FT_Int x, FT_Int y) {
        FT_Int  i, j, p, q;
        FT_Int  x_max = x + bitmap->width;
        FT_Int  y_max = y + bitmap->rows;
        // y = y - bitmap->rows;

        for ( i = x, p = 0; i < x_max; i++, p++ )
        {
            for ( j = y, q = 0; j < y_max; j++, q++ )
            {
                if ( i < 0      || j < 0       ||
                    i >= width || j >= height )
                    continue;

                image[j][i][3] |= bitmap->buffer[q * bitmap->width + p];
                image[j][i] = glm::u8vec4(color, image[j][i][3]);
            }
        }
    };

    for(auto& text: texts) {
            // break text by newlines
        std::vector<std::string> lines = wrapText(text.text, text.line_length);

        double current_x = text.start_pos.x;
        double current_y = text.start_pos.y;

        for(std::string& line: lines) {
            /* Create hb-buffer and populate. */
            hb_buffer_clear_contents(hb_buffer);
            // hb_buffer_reset(hb_buffer);
            hb_buffer_add_utf8 (hb_buffer, line.c_str(), -1, 0, -1);
            hb_buffer_guess_segment_properties (hb_buffer);
            // hb_buffer_set_direction(hb_buffer, HB_DIRECTION_LTR);
            // hb_buffer_set_script(hb_buffer, HB_SCRIPT_COMMON);
            // hb_buffer_set_language(hb_buffer, hb_language_from_string("en", -1));
            // hb_font_set_scale(hb_font, font_size * 64, font_size * 64);

            /* Shape it! */
            hb_shape (hb_font, hb_buffer, NULL, 0);

            /* Get glyph information and positions out of the buffer. */
            unsigned int len = hb_buffer_get_length(hb_buffer);
            hb_glyph_info_t *info = hb_buffer_get_glyph_infos(hb_buffer, NULL);
            hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(hb_buffer, NULL);

            /* Converted to absolute positions and draw the glyph to image array */
            {   
                for (unsigned int i = 0; i < len; i++) {   
                    // calculate position
                    hb_codepoint_t gid = info[i].codepoint;
                    // unsigned int cluster = info[i].cluster;
                    char glyphname[32];
                    hb_font_get_glyph_name (hb_font, gid, glyphname, sizeof (glyphname));
                    
                    /* load glyph image into the slot (erase previous one) */
                    if((ft_error = FT_Load_Glyph(ft_face, 
                    gid, // the glyph_index in the font file
                    FT_LOAD_DEFAULT))) {
                        continue;
                    }
                    if((ft_error = FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL))) {
                        continue;
                    }

                    double x_position = current_x + pos[i].x_offset / 64. + slot->bitmap_left;
                    double y_position = current_y + pos[i].y_offset / 64. - slot->bitmap_top;
                    // printf ("glyph='%s'	bitmap_top=%d	bitmap_left=%d  current_pos = (%f,%f)  y0=%f y1=%f rows=%d\n",
                    //     glyphname, slot->bitmap_top, slot->bitmap_left, current_x, current_y, 
                    //     y_position, 
                    //     current_y + pos[i].y_offset / 64. + slot->bitmap_top, 
                    //     slot->bitmap.rows);

                    current_x += pos[i].x_advance / 64.;
                    current_y += pos[i].y_advance / 64.;

                    draw_bitmap( &slot->bitmap,
                                x_position,
                                y_position);
                    
                }
            }
            current_x = text.start_pos.x;
            current_y += text.line_height;
        }
    }

    // {
    //     // show image
    //     int  i, j;
    //     for ( i = 0; i < height; i++ )
    //     {
    //         for ( j = 0; j < width; j++ )
    //             putchar( image[i][j] == 0 ? ' ' : image[i][j] < 128 ? '+' : '*' );
    //         putchar( '\n' );
    //     }
    // }

    glm::u8vec4* data = new glm::u8vec4[width*height];
    int k = 0;
    for (int i = height-1; i >= 0; i-- ){
        for (int j = 0; j < width; j++ )
            data[k++] = image[i][j];
    }

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        data
    );
    // set texture options
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    GL_ERRORS();

    delete[] data;    
}

std::vector<std::string> Font::wrapText(const std::string& text, size_t line_length) {
    std::vector<std::string> lines;
    size_t start = 0;
    
    while (start < text.length()) {
        // Find the position to break the line
        size_t end = start + line_length;
        
        if (end >= text.length()) {
            lines.push_back(text.substr(start));
            break;
        }

        // Find the last space within the line length to avoid cutting words
        size_t spacePos = text.rfind(' ', end);

        if (spacePos != std::string::npos && spacePos > start) {
            lines.push_back(text.substr(start, spacePos - start));
            start = spacePos + 1; // Move to the next word
            
        } else {
            // If no space is found, break at the line length (potential word split)
            lines.push_back(text.substr(start, line_length));
            start += line_length;
        }
    }

    return lines;
}