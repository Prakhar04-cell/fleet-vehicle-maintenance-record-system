#include "pdf_gen.h"
#include <stdarg.h>

#define MAX_PDF_PAGES 64
#define PAGE_WIDTH 595.28   /* A4 Width in points */
#define PAGE_HEIGHT 841.89  /* A4 Height in points */
#define STREAM_CAPACITY 65536

typedef struct {
    char *stream;
    size_t length;
    size_t capacity;
} PdfPage;

struct PdfDoc {
    PdfPage pages[MAX_PDF_PAGES];
    int page_count;
    int current_page;
    double current_font_size;
    bool is_bold;
};

PdfDoc *pdf_create(void) {
    PdfDoc *doc = (PdfDoc *)calloc(1, sizeof(PdfDoc));
    if (!doc) return NULL;
    doc->current_font_size = 10.0;
    doc->is_bold = false;
    pdf_new_page(doc);
    return doc;
}

void pdf_destroy(PdfDoc *doc) {
    if (!doc) return;
    for (int i = 0; i < doc->page_count; i++) {
        if (doc->pages[i].stream) {
            free(doc->pages[i].stream);
        }
    }
    free(doc);
}

void pdf_new_page(PdfDoc *doc) {
    if (!doc || doc->page_count >= MAX_PDF_PAGES) return;
    int idx = doc->page_count;
    doc->pages[idx].capacity = STREAM_CAPACITY;
    doc->pages[idx].stream = (char *)malloc(STREAM_CAPACITY);
    if (!doc->pages[idx].stream) return;
    doc->pages[idx].length = 0;
    doc->pages[idx].stream[0] = '\0';
    doc->page_count++;
    doc->current_page = idx;
}

static void append_to_stream(PdfDoc *doc, const char *fmt, ...) {
    if (!doc || doc->current_page < 0 || doc->current_page >= doc->page_count) return;
    PdfPage *p = &doc->pages[doc->current_page];

    char buf[1024];
    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (written <= 0) return;

    if (p->length + written + 1 >= p->capacity) {
        size_t new_cap = p->capacity * 2;
        char *new_stream = (char *)realloc(p->stream, new_cap);
        if (!new_stream) return;
        p->stream = new_stream;
        p->capacity = new_cap;
    }

    memcpy(p->stream + p->length, buf, written);
    p->length += written;
    p->stream[p->length] = '\0';
}

void pdf_set_font_size(PdfDoc *doc, double size) {
    if (doc) doc->current_font_size = size;
}

void pdf_set_font_bold(PdfDoc *doc, bool bold) {
    if (doc) doc->is_bold = bold;
}

void pdf_set_text_color(PdfDoc *doc, double r, double g, double b) {
    append_to_stream(doc, "%.3f %.3f %.3f rg\n", r, g, b);
}

void pdf_set_fill_color(PdfDoc *doc, double r, double g, double b) {
    append_to_stream(doc, "%.3f %.3f %.3f rg\n", r, g, b);
}

void pdf_set_stroke_color(PdfDoc *doc, double r, double g, double b) {
    append_to_stream(doc, "%.3f %.3f %.3f RG\n", r, g, b);
}

void pdf_draw_text(PdfDoc *doc, double x, double y, const char *text) {
    if (!doc || !text) return;

    /* Escape text: ( -> \(, ) -> \), \ -> \\ */
    char escaped[MAX_BUFFER_XXL];
    size_t e_idx = 0;
    for (const char *p = text; *p && e_idx + 2 < sizeof(escaped); p++) {
        if (*p == '(' || *p == ')' || *p == '\\') {
            escaped[e_idx++] = '\\';
        }
        escaped[e_idx++] = *p;
    }
    escaped[e_idx] = '\0';

    const char *font_name = doc->is_bold ? "/F2" : "/F1";
    append_to_stream(doc, "BT %s %.1f Tf %.2f %.2f Td (%s) Tj ET\n",
                     font_name, doc->current_font_size, x, y, escaped);
}

void pdf_draw_rect(PdfDoc *doc, double x, double y, double width, double height, bool fill, bool stroke) {
    if (!doc) return;
    const char *op = "n";
    if (fill && stroke) op = "B";
    else if (fill) op = "f";
    else if (stroke) op = "s";

    append_to_stream(doc, "%.2f %.2f %.2f %.2f re %s\n", x, y, width, height, op);
}

void pdf_draw_line(PdfDoc *doc, double x1, double y1, double x2, double y2, double line_width) {
    if (!doc) return;
    append_to_stream(doc, "%.2f w %.2f %.2f m %.2f %.2f l S\n", line_width, x1, y1, x2, y2);
}

StatusCode pdf_save_to_file(PdfDoc *doc, const char *filename) {
    if (!doc || !filename || doc->page_count == 0) return STATUS_INVALID_INPUT;

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "[PDF ERROR] Cannot create file: %s\n", filename);
        return STATUS_ERROR;
    }

    /* Track object byte offsets for xref table */
    long offsets[256];
    int obj_idx = 1;

    /* PDF Header */
    fprintf(fp, "%%PDF-1.4\n%%\xe2\xe3\xcf\xd3\n");

    /* Object 1: Catalog */
    offsets[obj_idx++] = ftell(fp);
    fprintf(fp, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");

    /* Object 2: Pages Tree */
    offsets[obj_idx++] = ftell(fp);
    fprintf(fp, "2 0 obj\n<< /Type /Pages /Kids [");
    for (int i = 0; i < doc->page_count; i++) {
        fprintf(fp, " %d 0 R", 5 + i * 2);
    }
    fprintf(fp, " ] /Count %d >>\nendobj\n", doc->page_count);

    /* Object 3: Font Helvetica (Regular) */
    offsets[obj_idx++] = ftell(fp);
    fprintf(fp, "3 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n");

    /* Object 4: Font Helvetica-Bold */
    offsets[obj_idx++] = ftell(fp);
    fprintf(fp, "4 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold >>\nendobj\n");

    /* Pages and content streams */
    for (int i = 0; i < doc->page_count; i++) {
        int page_obj = 5 + i * 2;
        int stream_obj = page_obj + 1;

        /* Page Object */
        offsets[obj_idx++] = ftell(fp);
        fprintf(fp, "%d 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %.2f %.2f] "
                    "/Resources << /Font << /F1 3 0 R /F2 4 0 R >> >> "
                    "/Contents %d 0 R >>\nendobj\n",
                page_obj, PAGE_WIDTH, PAGE_HEIGHT, stream_obj);

        /* Stream Object */
        offsets[obj_idx++] = ftell(fp);
        fprintf(fp, "%d 0 obj\n<< /Length %lu >>\nstream\n%sendstream\nendobj\n",
                stream_obj, (unsigned long)doc->pages[i].length, doc->pages[i].stream);
    }

    /* Cross-Reference Table */
    long xref_pos = ftell(fp);
    fprintf(fp, "xref\n0 %d\n", obj_idx);
    fprintf(fp, "0000000000 65535 f \n");
    for (int i = 1; i < obj_idx; i++) {
        fprintf(fp, "%010ld 00000 n \n", offsets[i]);
    }

    /* Trailer */
    fprintf(fp, "trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%ld\n%%%%EOF\n", obj_idx, xref_pos);
    fclose(fp);

    return STATUS_OK;
}
