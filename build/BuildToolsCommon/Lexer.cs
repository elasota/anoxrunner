using System.Text;

namespace BuildToolsCommon
{
    public enum TokenType
    {
        Identifier,
        Punctuation,
        Number,
        QuotedString,
    }

    public struct LexerState
    {
        public int Line;
        public int Col;
        public int Position;
    }

    public struct Token
    {
        public LexerState StartPosition;
        public int Length;
        public TokenType TokenType;

        public override string ToString()
        {
            return $"[{StartPosition.Line}:{StartPosition.Col}-{StartPosition.Col + Length}]";
        }
    }

    public interface ICharConverter<T>
    {
        static abstract int ToCharCode(T ch);
    }

    public class Lexer<TChar, TCharConverter, TList>
        where TCharConverter : ICharConverter<TChar>
        where TList : IList<TChar>
    {
        public LexerState State;

        TList _chars;

        public Lexer(TList chars)
        {
            _chars = chars;

            State.Line = 1;
            State.Col = 1;
            State.Position = 0;
        }

        public Token? GetToken()
        {
            while (true)
            {
                if (IsAtEnd())
                    return null;

                int firstChar = TCharConverter.ToCharCode(_chars[State.Position]);

                LexerState initialState = State;

                ConsumeChar(firstChar);

                if (IsWhitespace(firstChar))
                    continue;

                if (firstChar == '/')
                {
                    if (!IsAtEnd())
                    {
                        int secondChar = TCharConverter.ToCharCode(_chars[State.Position]);

                        if (secondChar == '*')
                        {
                            ConsumeChar(firstChar);
                            SkipBlockComment();
                        }
                        else if (secondChar == '/')
                        {
                            ConsumeChar(firstChar);
                            SkipLineComment();
                        }
                        else
                            return GetPunctuation(initialState, firstChar);
                    }
                }

                if (IsIdentifierStartChar(firstChar))
                    return GetIdentifier(initialState);
                else if (IsNumeric(firstChar))
                    return GetNumber(initialState);
                else if (firstChar == '\"')
                    return GetQuotedString(initialState);
                else
                    return GetPunctuation(initialState, firstChar);
            }
        }

        private bool IsWhitespace(int firstChar)
        {
            return firstChar >= 0 && firstChar <= ' ';
        }

        private bool IsAtEnd()
        {
            return _chars.Count == State.Position;
        }

        private void ConsumeChar(int charCode)
        {
            State.Position++;

            if (charCode == '\n')
            {
                State.Line++;
                State.Col = 1;
            }
            else if (charCode == '\r')
            {
                State.Line++;
                State.Col = 1;

                if (!IsAtEnd() && TCharConverter.ToCharCode(_chars[State.Position]) == '\n')
                    State.Position++;
            }
        }

        private Token? GetNumber(LexerState initialState)
        {
            while (!IsAtEnd())
            {
                int ch = TCharConverter.ToCharCode(_chars[State.Position]);

                if (IsNumeric(ch))
                    ConsumeChar(ch);
                else
                    break;
            }

            return CommitToken(initialState, TokenType.Number);
        }

        private Token? GetQuotedString(LexerState initialState)
        {
            while (!IsAtEnd())
            {
                int ch = TCharConverter.ToCharCode(_chars[State.Position]);
                ConsumeChar(ch);

                if (ch == '\"')
                    return CommitToken(initialState, TokenType.QuotedString);

                if (ch == '\\')
                    throw new NotImplementedException();
            }

            throw new Exception("Unexpected end of file while parsing quoted string");
        }

        private static bool IsAlpha(int ch)
        {
            return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
        }

        private static bool IsNumeric(int ch)
        {
            return (ch >= '0' && ch <= '9');
        }

        private static bool IsIdentifierStartChar(int ch)
        {
            return ch == '_' || IsAlpha(ch);
        }

        private Token GetIdentifier(LexerState initialState)
        {
            while (!IsAtEnd())
            {
                int ch = TCharConverter.ToCharCode(_chars[State.Position]);

                if (IsIdentifierStartChar(ch) || IsNumeric(ch))
                    ConsumeChar(ch);
                else
                    break;
            }

            return CommitToken(initialState, TokenType.Identifier);
        }

        private Token CommitToken(LexerState initialState, TokenType tokenType)
        {
            Token tok;
            tok.TokenType = tokenType;
            tok.StartPosition = initialState;
            tok.Length = State.Position - initialState.Position;

            return tok;
        }

        private Token? GetPunctuation(LexerState initialState, int ch)
        {
            switch (ch)
            {
                case '@':
                case '#':
                case '$':
                case '(':
                case ')':
                case '{':
                case '}':
                case '[':
                case ']':
                case ';':
                case ',':
                case '?':
                case '.':
                    // Single-character punctuation
                    return CommitToken(initialState, TokenType.Punctuation);
                case '^':
                case '!':
                case '/':
                case '%':
                case '*':
                    // Continuable punctuation
                    return GetContinuablePunctuation(initialState, null, "=");
                case '-':
                case '+':
                case '=':
                case '|':
                case '&':
                    // Repeatable continuable punctuation
                    return GetContinuablePunctuation(initialState, ch, "=");
                case ':':
                case '<':
                case '>':
                    // Repeatable punctuation
                    return GetContinuablePunctuation(initialState, ch, "");
                default:
                    throw new LexException(initialState, "Unrecognized punctuation character '" + ((char)ch).ToString() + "'");
            }
        }

        private Token? GetContinuablePunctuation(LexerState initialState, int? repeatCh, string continuations)
        {
            if (!IsAtEnd())
            {
                int nextCh = TCharConverter.ToCharCode(_chars[State.Position]);

                bool isContinuation = false;
                if (repeatCh.HasValue && repeatCh.Value == nextCh)
                    isContinuation = true;
                else
                {
                    foreach (char continuationCh in continuations)
                    {
                        if (continuationCh == nextCh)
                        {
                            isContinuation = true;
                            break;
                        }
                    }
                }

                if (isContinuation)
                    ConsumeChar(nextCh);
            }

            return CommitToken(initialState, TokenType.Punctuation);
        }

        private void SkipLineComment()
        {
            while (!IsAtEnd())
            {
                int ch = TCharConverter.ToCharCode(_chars[State.Position]);

                if (ch == '\r' || ch == '\n')
                    return;
            }
        }

        private void SkipBlockComment()
        {
            while (!IsAtEnd())
            {
                int ch = TCharConverter.ToCharCode(_chars[State.Position]);

                ConsumeChar(ch);

                if (ch == '*')
                {
                    return;
                }
            }
        }

        public string TokenToString(Token token)
        {
            StringBuilder sb = new StringBuilder();

            int startOffset = token.StartPosition.Position;

            for (int i = 0; i < token.Length; i++)
                sb.Append((char)TCharConverter.ToCharCode(_chars[i + startOffset]));

            return sb.ToString();
        }

        public bool TokenIsString(Token token, string str)
        {
            int len = str.Length;
            if (token.Length != len)
                return false;

            int startOffset = token.StartPosition.Position;

            for (int i = 0; i < token.Length; i++)
            {
                if (TCharConverter.ToCharCode(_chars[i + startOffset]) != str[i])
                    return false;
            }

            return true;
        }

        public string? UnquoteString(Token token)
        {
            StringBuilder sb = new StringBuilder();

            int startOffset = token.StartPosition.Position;

            int i = token.StartPosition.Position + 1;
            int endPos = token.StartPosition.Position + token.Length - 1;

            while (i < endPos)
            {
                int charCode = (char)TCharConverter.ToCharCode(_chars[i]);

                char ch = (char)charCode;
                i++;

                sb.Append(ch);
            }

            return sb.ToString();
        }
    }

    [Serializable]
    internal class LexException : Exception
    {
        private LexerState InitialState { get; }

        public LexException()
        {
        }

        public LexException(string? message) : base(message)
        {
        }

        public LexException(LexerState initialState, string message)
            : base(message)
        {
            this.InitialState = initialState;
        }

        public LexException(string? message, Exception? innerException) : base(message, innerException)
        {
        }
    }
}
