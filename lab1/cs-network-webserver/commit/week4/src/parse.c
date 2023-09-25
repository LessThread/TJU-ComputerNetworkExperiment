#include "parse.h"

/**
* Given a char buffer returns the parsed request headers
*/
Request *parse(char *buffer, int size, int socketFd)
{
	//Differant states in the state machine
	enum
	{
		STATE_START = 0,
		STATE_CR,
		STATE_CRLF,
		STATE_CRLFCR,
		STATE_CRLFCRLF
	};

	int i = 0, state;
	size_t offset = 0;
	char ch;
	char buf[8192];
	memset(buf, 0, 8192);

	int crlf_num = 0;
	int headers_num=0;

	state = STATE_START;
	while (state != STATE_CRLFCRLF)
	{
		char expected = 0;

		if (i == size)
			break;

		ch = buffer[i++];
		buf[offset++] = ch;

		switch (state)
		{
		case STATE_START:
		case STATE_CRLF:
			expected = '\r';
			break;
		case STATE_CR:
		case STATE_CRLFCR:
			crlf_num += 1;
			expected = '\n';
			break;
		default:
			state = STATE_START;
			continue;
		}

		if (ch == expected)
			state++;
		else
			state = STATE_START;
	}

	//Valid End State
	if (state == STATE_CRLFCRLF)
	{
		Request *request = (Request *)malloc(sizeof(Request));
		request->header_count = 0;
		//TODO You will need to handle resizing this in parser.y

		/*
			这是一段HTTP请求的代码。这段代码包含了一个名为request_no_tcrlf的规则，该规则匹配不包含回车换行符的HTTP请求。
			如果该规则匹配到了多个头部，就会打印“parsing_request: Matched request_no_tcrlf with multiple headers.”，
			否则就会打印“parsing_request: Matched request_no_tcrlf with no headers.”。
			如果该规则没有匹配到任何头部，就会匹配到request_line规则，打印“parsing_request: Matched request_no_tcrlf with no headers.”。
			最后，如果该规则匹配到了一个HTTP请求，就会打印“parsing_request: Matched request rule1.”。
			其中request_header和request_line是HTTP请求中的两个部分。
			request_header包含了请求的头部信息，而request_line包含了请求的第一行信息。t_crlf是回车换行符。
		*/

		if(crlf_num-2<=0)
			headers_num=0;
		else
			headers_num=crlf_num-2;

		//printf("headers_num:%d\n",headers_num);
		
		
		
		request->headers = (Request_header *)malloc(sizeof(Request_header) * headers_num);
		set_parsing_options(buf, i, request);

		if (yyparse() == SUCCESS)
		{
			return request;
		}
	}
	//TODO Handle Malformed Requests
	YY_BUFFER_STATE bufferstate = yy_create_buffer(NULL, 16384);
	yy_switch_to_buffer(bufferstate);
	printf("Parsing Failed\n");
	return NULL;
}
