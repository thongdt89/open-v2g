# Bounded typed service parameters

Draft 0.1 — 31/10/2026. Host generated-codec behavior; imported AUTOSAR and
protocol generated types are unchanged. No selected service policy is implied.

The project parameter DTO now distinguishes boolean, signed byte, signed short,
integer, physical and string choices. Kind zero preserves the earlier integer/
`is_string` convenience convention. Explicit kind takes precedence over that
legacy hint. Width checks reject out-of-range boolean/byte/short values before
encoding; decoded choices must have exactly one value. Names and strings remain
bounded to 32 bytes and total parameter count remains five.

Physical parameters retain a signed int16 mantissa, explicit signed decimal
multiplier and explicit unit. This representation preserves opaque service data;
it does not silently convert unknown service parameters into charging limits.
The host multiplier range is -3..3. ISO-2 supports hour/metre/second/ampere/volt/
watt/watt-hour units; DIN additionally supports ampere-hour, volt-ampere and
watt-second. Mappings are explicit switches, not cross-schema enum casts.
DIN physical units must be present, and its ValueType must match the value choice.

ISO-2 parameters with the same SetID are grouped into one generated set, with at
most five parameters across all sets. Duplicate names in a set or duplicate
encoded set IDs are rejected; equal names in different sets are allowed. Decode
flattens set order and parameter order into owned bounded DTO entries. The pinned
DIN model carries one Parameter per set, so duplicate DIN set IDs are rejected
instead of inventing a multi-parameter layout.

The station fixture now sends integer, boolean and physical parameters in real
ServiceDetail socket responses. The client parses/copies them but still uses its
explicit existing charging-service selection; arbitrary parameter names cannot
change permission, actuators or authorization. Broader offers, selection policy,
tariffs and charging profiles remain separate work.

`openv2g_test_cbv2g_parameters` checks both schemas, each value choice, signed
bounds, unit restrictions, duplicate/capacity rejection, grouping, and truncated
input with unchanged output. Two additional bundles independently decode and
re-encode byte-identically through the separate pinned oracle. Earlier 27 feature
and 50 baseline vectors remain unchanged. See
[executed verification](../../docs/architecture/evidence/service_parameter_verification.json).
