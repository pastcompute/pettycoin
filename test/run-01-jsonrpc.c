#include <stdio.h>
#include <stdarg.h>
#include <ccan/io/io.h>
#include <ccan/tal/tal.h>

#include "../log.h"

#undef log_unusual
#undef log_info
#define log_unusual(...)
#define log_info(...)

#include "../jsonrpc.c"
#include "../json.c"

void test(const char *input, const char *expect, bool needs_more, int extra)
{
	struct json_connection *jcon = tal(NULL, struct json_connection);
	struct io_plan plan;

	jcon->used = 0;
	jcon->len_read = strlen(input);
	jcon->buffer = tal_dup(jcon, char, input, strlen(input), 0);
	list_head_init(&jcon->output);

	plan = read_json(NULL, jcon);
	if (needs_more) {
		/* Should have done partial read for rest. */
		assert(jcon->used == strlen(input));
		assert(plan.next == (void *)read_json);
		assert(plan.u1.cp == jcon->buffer + strlen(input));
		assert(list_empty(&jcon->output));
	} else if (!expect) {
		/* Should have returned io_close. */
		assert(plan.next == NULL);
	} else {
		/* Should have finished. */
		assert(jcon->used == extra);
		assert(plan.next == (void *)read_json);
		assert(!list_empty(&jcon->output));
		assert(streq(list_top(&jcon->output, struct json_output, list)
			     ->json, expect));
	}
	tal_free(jcon);
}	

int main(void)
{
	unsigned int i;
	const char *cmd;
	const char echocmd[] = "{ \"method\" : \"dev-echo\", "
		"\"params\" : [ \"hello\", \"Arabella!\" ], "
		"\"id\" : \"1\" }";
	const char echoresult[]
		= "{ \"result\" : { \"num\" : 2,"
		" [ \"hello\", \"Arabella!\" ] }, "
		"\"error\" : null, \"id\" : \"1\" }\n";

	/* Make partial commands work. */
	for (i = 1; i < strlen(echocmd); i++) {
		cmd = tal_strndup(NULL, echocmd, i);
		test(cmd, NULL, true, 0);
		tal_free(cmd);
	}

	test(echocmd, echoresult, false, 0);

	/* Two commands at once will also work (second will be left in buf) */
	cmd = tal_fmt(NULL, "%s%s", echocmd, echocmd);

	test(cmd, echoresult, false, strlen(echocmd));
	tal_free(cmd);

	/* Unknown method. */
	test("{ \"method\" : \"unknown\", "
	     "\"params\" : [ \"hello\", \"Arabella!\" ], "
	     "\"id\" : \"2\" }",
	     "{ \"result\" : null, "
	     "\"error\" : \"Unknown command 'unknown'\", \"id\" : \"2\" }\n",
	     false, 0);

	/* Missing parts, will fail. */
	test("{ \"params\" : [ \"hello\", \"Arabella!\" ], "
	     "\"id\" : \"2\" }", NULL, false, 0);
	test("{ \"method\" : \"echo\", "
	     "\"id\" : \"2\" }", NULL, false, 0);
	test("{ \"method\" : \"echo\", "
	     "\"params\" : [ \"hello\", \"Arabella!\" ] }", NULL, false, 0);

	/* It doesn't help to have them in successive commands. */
	test("{ \"params\" : [ \"hello\", \"Arabella!\" ], "
	     "\"id\" : \"2\" }"
	     "{ \"method\" : \"unknown\", "
	     "\"params\" : [ \"hello\", \"Arabella!\" ], "
	     "\"id\" : \"2\" }", NULL, false, 0);

	return 0;
}
