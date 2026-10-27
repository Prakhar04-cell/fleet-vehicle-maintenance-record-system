#ifndef PDF_GEN_H
#define PDF_GEN_H

#include "common.h"

typedef struct PdfDoc PdfDoc;

PdfDoc *pdf_create(void);
void pdf_destroy(PdfDoc *doc);

void pdf_new_page(PdfDoc *doc);
void pdf_set_font_size(PdfDoc *doc, double size);
void pdf_set_font_bold(PdfDoc *doc, bool bold);
void pdf_set_text_color(PdfDoc *doc, double r, double g, double b);
void pdf_set_fill_color(PdfDoc *doc, double r, double g, double b);
void pdf_set_stroke_color(PdfDoc *doc, double r, double g, double b);

void pdf_draw_text(PdfDoc *doc, double x, double y, const char *text);
void pdf_draw_rect(PdfDoc *doc, double x, double y, double width, double height, bool fill, bool stroke);
void pdf_draw_line(PdfDoc *doc, double x1, double y1, double x2, double y2, double line_width);

StatusCode pdf_save_to_file(PdfDoc *doc, const char *filename);

#endif /* PDF_GEN_H */
