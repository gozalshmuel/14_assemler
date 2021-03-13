#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "writefiles.h"
#include "utils.h"
#include "first_pass.h"
#include "second_pass.h"
#include "code.h"

/**
 * Processes a single assembly source file, and returns the result status.
 * @param filename The filename, without it's extension
 * @return Whether succeeded
 */
static bool process_file(char *filename);

void merge_data_and_code_img(machine_word** memory_img, long* data_img, long icf, long dcf);

/**
 * Entry point - 24bit assembler. Assembly language specified in booklet.
 */
int main(int argc, char *argv[]) {
	int i;

	/* To break line if needed */
	bool succeeded = TRUE;
	printf("argc: %d\n", argc-1);

	/* Process each file by arguments */
	for (i = 1; argv[i] != NULL; ++i) {
		printf("\nfile[%d] is: %s\n", i,argv[i]);

		/* foreach argument (file name), send it for full processing. */
		succeeded = process_file(argv[i]);
		/* if last process failed and there's another file, break line: */
		if (!succeeded) puts("File - Failed\n");
		if (succeeded) puts("File - Succeeded\n");
	}
	return 0;
}

void merge_data_and_code_img(machine_word** memory_img, long* data_img, long icf, long dcf) {
	int i;
	machine_word* word_to_write;
	data_word* dataword;

	for (i = 0; i < dcf; i++) {
		word_to_write = (machine_word*)calloc_with_check(sizeof(machine_word));
		word_to_write->length = 0; /* Not Code word! */
		dataword = (data_word*)calloc_with_check(sizeof(data_word));
		dataword->ARE = A_MEM;	/* data word is always "A" */
		dataword->data = data_img[i]; /* insert the data into "dataword" */
		word_to_write->word.data = dataword;
		memory_img[icf - IC_INIT_VALUE + i] = word_to_write;
	}
}

static bool process_file(char *filename) {
	/* Memory address counters */
	int temp_c;
	long ic = IC_INIT_VALUE, dc = 0, icf, dcf;
	bool is_success = TRUE; /* is succeeded so far */
	char *input_filename = NULL;
	char temp_line[MAX_LINE_LENGTH + 2]; /* temporary string for storing line, read from file */
	FILE *file_des; /* Current assembly file descriptor to process */
	long data_img[CODE_ARR_IMG_LENGTH]; /* Contains an image of the machine code */
	machine_word *memory_img[CODE_ARR_IMG_LENGTH];
	/* Our symbol table */
	table symbol_table = NULL;
	line_info curr_line_info;

	input_filename = strtok(filename, ".");

	/* Concat extensionless filename with .as extension */
	input_filename = strallocat(filename, ".as");
	//input_filename = "fpass_errors.as"; //debug
	/* Open file, skip on failure */
	file_des = fopen(input_filename, "r");
	if (file_des == NULL) {
		/* if file couldn't be opened, write to stderr. */
		printf("Error: file \"%s\" is inaccessible for reading. skipping it.\n", filename);
		free(input_filename); /* The only allocated space is for the full file name */
		return FALSE;
	}

	/* start first pass: */
	curr_line_info.file_name = input_filename;
	curr_line_info.content = temp_line; /* We use temp_line to read from the file, but it stays at same location. */
	/* Read line - stop if read failed (when NULL returned) - usually when EOF. increase line counter for error printing. */
	for (curr_line_info.line_number = 1;
	     fgets(temp_line, MAX_LINE_LENGTH + 2, file_des) != NULL; curr_line_info.line_number++) {
		/* if line too long, the buffer doesn't include the '\n' char OR the file isn't on end. */
		if (strchr(temp_line, '\n') == NULL && !feof(file_des)) {
			/* Print message and prevent further line processing, as well as second pass.  */
			printf_line_error(curr_line_info, "Line too long to process. Maximum line length should be %d.",
			                  MAX_LINE_LENGTH);
			is_success = FALSE;
			/* skip leftovers */
			do {
				temp_c = fgetc(file_des);
			} while (temp_c != '\n' && temp_c != EOF);
		} else {
			if (!process_line_fpass(curr_line_info, &ic, &dc, memory_img, data_img, &symbol_table)) {
				if (is_success) {
					/*free_code_image(memory_img, ic_before);*/
					icf = -1;
					is_success = FALSE;
				}
			}
		}
	}

	/* Save ICF & DCF */
	icf = ic;
	dcf = dc;

	/* Merge data image and code image at the end of the memory image */
	merge_data_and_code_img(memory_img, data_img, icf, dcf);


	/* if first pass didn't fail, start the second pass */
	if (is_success) {

		ic = IC_INIT_VALUE;

		/* Now let's add IC to each DC for each of the data symbols in table (step 1.19) */
		add_value_to_type(symbol_table, icf, DATA_SYMBOL);

		/* First pass done right. start second pass: */
		rewind(file_des); /* Start from beginning of file again */

		for (curr_line_info.line_number = 1; !feof(file_des); curr_line_info.line_number++) {
			int i = 0;
			fgets(temp_line, MAX_LINE_LENGTH, file_des); /* Get line */
			MOVE_TO_NOT_WHITE(temp_line, i)
			if (memory_img[ic - IC_INIT_VALUE] != NULL || temp_line[i] == '.')
				is_success &= process_line_spass(curr_line_info, &ic, memory_img, &symbol_table);
		}

			/* Write files if second pass succeeded */
			if (is_success) {
				/* Everything was done. Write to *filename.ob/.ext/.ent */
				is_success = write_output_files(memory_img, data_img, icf, dcf, filename, symbol_table);
			}
	}

	/* Free symbol table */
	free_table(symbol_table);
	/* Free code & data buffer contents */
	free_code_image(memory_img, icf);

	/* return whether every assembling succeeded */
	return is_success;
}