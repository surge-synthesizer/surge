    .section ".rodata", "a"

configurationXmlStart:
    .globl configurationXmlStart
    .incbin "../../resources/data/configuration.xml"
configurationXmlEnd:
    .global configurationXmlNullTerminator
    .type configurationXmlNullTerminator, %object
configurationXmlNullTerminator:
    .int 0
