// Code generated by easyjson for marshaling/unmarshaling. DO NOT EDIT.

package netlink

import (
	json "encoding/json"

	easyjson "github.com/mailru/easyjson"
	jlexer "github.com/mailru/easyjson/jlexer"
	jwriter "github.com/mailru/easyjson/jwriter"
)

// suppress unused package warning
var (
	_ *json.RawMessage
	_ *jlexer.Lexer
	_ *jwriter.Writer
	_ easyjson.Marshaler
)

func easyjsonF642ad3eDecodeGithubComDataDogDatadogAgentPkgEbpfNetlink(in *jlexer.Lexer, out *IPTranslation) {
	isTopLevel := in.IsStart()
	if in.IsNull() {
		if isTopLevel {
			in.Consumed()
		}
		in.Skip()
		return
	}
	in.Delim('{')
	for !in.IsDelim('}') {
		key := in.UnsafeString()
		in.WantColon()
		if in.IsNull() {
			in.Skip()
			in.WantComma()
			continue
		}
		switch key {
		case "r_src":
			out.ReplSrcIP = string(in.String())
		case "r_dst":
			out.ReplDstIP = string(in.String())
		case "r_sport":
			out.ReplSrcPort = uint16(in.Uint16())
		case "r_dport":
			out.ReplDstPort = uint16(in.Uint16())
		default:
			in.SkipRecursive()
		}
		in.WantComma()
	}
	in.Delim('}')
	if isTopLevel {
		in.Consumed()
	}
}
func easyjsonF642ad3eEncodeGithubComDataDogDatadogAgentPkgEbpfNetlink(out *jwriter.Writer, in IPTranslation) {
	out.RawByte('{')
	first := true
	_ = first
	{
		const prefix string = ",\"r_src\":"
		if first {
			first = false
			out.RawString(prefix[1:])
		} else {
			out.RawString(prefix)
		}
		out.String(string(in.ReplSrcIP))
	}
	{
		const prefix string = ",\"r_dst\":"
		if first {
			first = false
			out.RawString(prefix[1:])
		} else {
			out.RawString(prefix)
		}
		out.String(string(in.ReplDstIP))
	}
	{
		const prefix string = ",\"r_sport\":"
		if first {
			first = false
			out.RawString(prefix[1:])
		} else {
			out.RawString(prefix)
		}
		out.Uint16(uint16(in.ReplSrcPort))
	}
	{
		const prefix string = ",\"r_dport\":"
		if first {
			first = false
			_ = first
			out.RawString(prefix[1:])
		} else {
			out.RawString(prefix)
		}
		out.Uint16(uint16(in.ReplDstPort))
	}
	out.RawByte('}')
}

// MarshalJSON supports json.Marshaler interface
func (v IPTranslation) MarshalJSON() ([]byte, error) {
	w := jwriter.Writer{}
	easyjsonF642ad3eEncodeGithubComDataDogDatadogAgentPkgEbpfNetlink(&w, v)
	return w.Buffer.BuildBytes(), w.Error
}

// MarshalEasyJSON supports easyjson.Marshaler interface
func (v IPTranslation) MarshalEasyJSON(w *jwriter.Writer) {
	easyjsonF642ad3eEncodeGithubComDataDogDatadogAgentPkgEbpfNetlink(w, v)
}

// UnmarshalJSON supports json.Unmarshaler interface
func (v *IPTranslation) UnmarshalJSON(data []byte) error {
	r := jlexer.Lexer{Data: data}
	easyjsonF642ad3eDecodeGithubComDataDogDatadogAgentPkgEbpfNetlink(&r, v)
	return r.Error()
}

// UnmarshalEasyJSON supports easyjson.Unmarshaler interface
func (v *IPTranslation) UnmarshalEasyJSON(l *jlexer.Lexer) {
	easyjsonF642ad3eDecodeGithubComDataDogDatadogAgentPkgEbpfNetlink(l, v)
}
