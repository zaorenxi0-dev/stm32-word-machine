/**
 ****************************************************************************************************
 * @file        calc.c
 * @author      zhangguangmao
 * @version     V1.0
 * @date        2025-12-20
 * @brief       简易计算器核心：基于「双栈算法」的中缀表达式求值（实现文件）
 * @license     Copyright (c) 2025
 ****************************************************************************************************
 */

#include "calc.h"

/* ------------------------------------------------------------------------------------------------
 * 内部函数
 * ---------------------------------------------------------------------------------------------- */

/**
 * @brief  取运算符优先级
 * @param  op : 运算符
 * @retval 2 -> * /      1 -> + -      0 -> ( （左括号不入栈参与比较）
 */
static int calc_priority(char op)
{
    switch (op)
    {
        case '*':
        case '/':
            return 2;
        case '+':
        case '-':
            return 1;
        default:
            return 0;       /* '(' */
    }
}

/**
 * @brief  弹出栈顶运算符与两个操作数，完成一次二元运算并把结果压回操作数栈
 * @param  op        : 运算符
 * @param  opnd      : 操作数栈
 * @param  opnd_top  : 操作数栈元素个数（指针传入，函数内修改）
 * @retval CALC_OK 或错误码
 */
static int calc_pop_calc(char op, double *opnd, int *opnd_top)
{
    double a;
    double b;
    double r;

    if (*opnd_top < 2)
    {
        return CALC_ERR_SYNTAX;                 /* 操作数不足，表达式非法 */
    }

    b = opnd[--(*opnd_top)];                    /* 右操作数先出栈 */
    a = opnd[--(*opnd_top)];                    /* 左操作数后出栈 */

    switch (op)
    {
        case '+':
            r = a + b;
            break;
        case '-':
            r = a - b;
            break;
        case '*':
            r = a * b;
            break;
        case '/':
            if (b == 0.0)
            {
                return CALC_ERR_DIV_ZERO;       /* 除零保护 */
            }
            r = a / b;
            break;
        default:
            return CALC_ERR_SYNTAX;
    }

    opnd[(*opnd_top)++] = r;                    /* 结果压回操作数栈 */
    return CALC_OK;
}

/* ------------------------------------------------------------------------------------------------
 * 对外接口
 * ---------------------------------------------------------------------------------------------- */

int calc_eval(const char *expr, double *out)
{
    double opnd[CALC_MAX_OPERAND];              /* 操作数栈 */
    char   optr[CALC_MAX_OPERATOR];             /* 运算符栈 */
    int    opnd_top = 0;                        /* 操作数栈元素个数 */
    int    optr_top = 0;                        /* 运算符栈元素个数 */
    int    expect_operand = 1;                  /* 1: 当前应读到一个操作数（或左括号） */
    const char *p;
    char   c;
    int    rc;
    double value;
    int    has_digit;
    double scale;

    if ((expr == 0) || (out == 0))
    {
        return CALC_ERR_EMPTY;
    }

    p = expr;

    /* 跳过前导空白，若整个表达式为空则直接返回 */
    while ((*p == ' ') || (*p == '\t'))
    {
        p++;
    }
    if (*p == '\0')
    {
        return CALC_ERR_EMPTY;
    }

    while (*p != '\0')
    {
        c = *p;

        /* 1. 跳过空白字符 */
        if ((c == ' ') || (c == '\t'))
        {
            p++;
            continue;
        }

        /* 2. 一元正负号：在表达式开头、'(' 之后、或二元运算符之后出现 */
        if ((expect_operand != 0) && ((c == '+') || (c == '-')))
        {
            p++;
            if (c == '-')
            {
                /* 一元负号改写为 0 - x：先压 0，再把 '-' 当成普通运算符 */
                if (opnd_top >= CALC_MAX_OPERAND)
                {
                    return CALC_ERR_OPND_FULL;
                }
                if (optr_top >= CALC_MAX_OPERATOR)
                {
                    return CALC_ERR_OPTR_FULL;
                }
                opnd[opnd_top++] = 0.0;
                optr[optr_top++] = '-';
            }
            /* 一元正号无实际作用，直接忽略 */
            expect_operand = 1;
            continue;
        }

        /* 3. 左括号：直接入运算符栈 */
        if (c == '(')
        {
            if (expect_operand == 0)
            {
                return CALC_ERR_SYNTAX;         /* 形如 2( 的写法非法 */
            }
            if (optr_top >= CALC_MAX_OPERATOR)
            {
                return CALC_ERR_OPTR_FULL;
            }
            optr[optr_top++] = '(';
            p++;
            continue;
        }

        /* 4. 右括号：弹出运算符计算，直到遇到左括号 */
        if (c == ')')
        {
            if (expect_operand != 0)
            {
                return CALC_ERR_SYNTAX;         /* 形如 () 或 1+) 的写法非法 */
            }

            while ((optr_top > 0) && (optr[optr_top - 1] != '('))
            {
                rc = calc_pop_calc(optr[--optr_top], opnd, &opnd_top);
                if (rc != CALC_OK)
                {
                    return rc;
                }
            }

            if (optr_top == 0)
            {
                return CALC_ERR_PAREN;          /* 没有与之匹配的左括号 */
            }
            optr_top--;                         /* 弹出左括号 */
            p++;
            expect_operand = 0;
            continue;
        }

        /* 5. 数字：解析出完整数值（含小数）后压入操作数栈 */
        if (((c >= '0') && (c <= '9')) || (c == '.'))
        {
            if (expect_operand == 0)
            {
                return CALC_ERR_SYNTAX;         /* 形如 2 3 的写法非法 */
            }

            value = 0.0;
            has_digit = 0;

            while ((*p >= '0') && (*p <= '9'))  /* 整数部分 */
            {
                value = value * 10.0 + (double)(*p - '0');
                has_digit = 1;
                p++;
            }

            if (*p == '.')                      /* 小数部分 */
            {
                p++;
                scale = 0.1;
                while ((*p >= '0') && (*p <= '9'))
                {
                    value += (double)(*p - '0') * scale;
                    scale *= 0.1;
                    has_digit = 1;
                    p++;
                }
            }

            if (has_digit == 0)
            {
                return CALC_ERR_SYNTAX;         /* 只有一个孤立的小数点 */
            }

            if (opnd_top >= CALC_MAX_OPERAND)
            {
                return CALC_ERR_OPND_FULL;
            }
            opnd[opnd_top++] = value;
            expect_operand = 0;
            continue;
        }

        /* 6. 四则运算符：先弹出所有优先级不低于它的运算符，再入栈 */
        if ((c == '+') || (c == '-') || (c == '*') || (c == '/'))
        {
            if (expect_operand != 0)
            {
                return CALC_ERR_SYNTAX;         /* 形如 1+*2 的写法非法 */
            }

            while ((optr_top > 0) && (optr[optr_top - 1] != '(') &&
                   (calc_priority(optr[optr_top - 1]) >= calc_priority(c)))
            {
                rc = calc_pop_calc(optr[--optr_top], opnd, &opnd_top);
                if (rc != CALC_OK)
                {
                    return rc;
                }
            }

            if (optr_top >= CALC_MAX_OPERATOR)
            {
                return CALC_ERR_OPTR_FULL;
            }
            optr[optr_top++] = c;
            p++;
            expect_operand = 1;
            continue;
        }

        /* 7. 其它字符一律视为非法 */
        return CALC_ERR_SYNTAX;
    }

    if (expect_operand != 0)
    {
        return CALC_ERR_SYNTAX;                 /* 表达式以运算符结尾，如 "1+" */
    }

    /* 8. 弹出剩余运算符，完成全部计算 */
    while (optr_top > 0)
    {
        if (optr[optr_top - 1] == '(')
        {
            return CALC_ERR_PAREN;              /* 左括号没有闭合 */
        }
        rc = calc_pop_calc(optr[--optr_top], opnd, &opnd_top);
        if (rc != CALC_OK)
        {
            return rc;
        }
    }

    if (opnd_top != 1)
    {
        return CALC_ERR_SYNTAX;
    }

    *out = opnd[0];
    return CALC_OK;
}

void calc_format(double value, char *buf, unsigned int buf_size)
{
    char          num[40];
    char          idig[16];
    char          frac[CALC_FRAC_DIGITS];
    int           n = 0;
    int           i;
    int           f;
    int           m;
    int           neg;
    double        v;
    unsigned long ip;                           /* 整数部分 */
    unsigned long fp;                           /* 小数部分（已放大） */
    unsigned long scale = 1UL;

    if ((buf == 0) || (buf_size < 8))
    {
        return;
    }

    neg = (value < 0.0) ? 1 : 0;
    v   = neg ? -value : value;

    if ((v != v) || (v >= 1.0e9))               /* 处理 NaN 与超大数值 */
    {
        const char *ov = "OVERFLOW";
        unsigned int k = 0;
        while ((ov[k] != '\0') && (k < (buf_size - 1U)))
        {
            buf[k] = ov[k];
            k++;
        }
        buf[k] = '\0';
        return;
    }

    for (i = 0; i < CALC_FRAC_DIGITS; i++)
    {
        scale *= 10UL;
    }

    v  = v * (double)scale;                     /* 放大到整数域，便于拆分 */
    ip = (unsigned long)(v / (double)scale);    /* 整数部分（截断） */
    fp = (unsigned long)(v - (double)ip * (double)scale + 0.5);  /* 小数部分 + 四舍五入 */
    if (fp >= scale)                            /* 小数部分进位到整数部分 */
    {
        fp -= scale;
        ip += 1UL;
    }

    /* 小数部分：低位在前取出，随后裁掉末尾的 0（frac[] 头部即最低位） */
    for (f = 0; f < CALC_FRAC_DIGITS; f++)
    {
        frac[f] = (char)('0' + (int)(fp % 10UL));
        fp /= 10UL;
    }
    f = 0;
    while ((f < CALC_FRAC_DIGITS) && (frac[f] == '0'))
    {
        f++;
    }

    /* 整数部分：同样低位在前取出，输出时反向 */
    m = 0;
    if (ip == 0UL)
    {
        idig[m++] = '0';
    }
    else
    {
        while (ip > 0UL)
        {
            idig[m++] = (char)('0' + (int)(ip % 10UL));
            ip /= 10UL;
        }
    }

    if (neg != 0)
    {
        num[n++] = '-';
    }
    while (m > 0)
    {
        num[n++] = idig[--m];
    }
    if (f < CALC_FRAC_DIGITS)                   /* 还有有效小数位 */
    {
        num[n++] = '.';
        for (i = CALC_FRAC_DIGITS - 1; i >= f; i--)
        {
            num[n++] = frac[i];
        }
    }
    num[n] = '\0';

    for (i = 0; (i < n) && (i < ((int)buf_size - 1)); i++)
    {
        buf[i] = num[i];
    }
    buf[i] = '\0';
}

const char *calc_err_str(int code)
{
    switch (code)
    {
        case CALC_OK:
            return "OK";
        case CALC_ERR_EMPTY:
            return "EMPTY";
        case CALC_ERR_SYNTAX:
            return "SYNTAX";
        case CALC_ERR_PAREN:
            return "PAREN";
        case CALC_ERR_DIV_ZERO:
            return "DIV BY ZERO";
        case CALC_ERR_OVERFLOW:
            return "OVERFLOW";
        case CALC_ERR_OPND_FULL:
        case CALC_ERR_OPTR_FULL:
            return "STACK FULL";
        default:
            return "ERROR";
    }
}
