<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:template match="/">
<html>
<body>
<h2>Session List:</h2>
<table cellspacing="0" cellpading="0" bordercolor="#cccccc" border="1">
<tr bgcolor="#9acd32">
<th align="center">remote_ip</th>
<th align="center">remote_port</th>
<th align="center">session_type</th>
<th align="center">upload_bytes</th>
<th align="center">download_bytes</th>
<th align="center">begin_time</th>
<th align="center">end_time</th>
<th align="center">upload_rate</th>
<th align="center">download_rate</th>
</tr>

<xsl:for-each select="sessions/session">
<tr>
	<xsl:if test="position() mod 2 = 1">
		<xsl:attribute name="bgcolor">#EEEEEE</xsl:attribute>
	</xsl:if>
<td align="left"><xsl:value-of select="@remote_ip" /></td>
<td align="right"><xsl:value-of select="@remote_port" /></td>
<td align="right"><xsl:value-of select="@session_type" /></td>
<td align="right"><xsl:value-of select="@upload_bytes" /></td>
<td align="right"><xsl:value-of select="@download_bytes" /></td>
<td align="right"><xsl:value-of select="@begin_time" /></td>
<td align="right"><xsl:value-of select="@end_time" /></td>
<td align="right"><xsl:value-of select="@upload_rate" /></td>
<td align="right"><xsl:value-of select="@download_rate" /></td>
</tr>

</xsl:for-each>
</table>
</body>
</html>
</xsl:template>

</xsl:stylesheet>

