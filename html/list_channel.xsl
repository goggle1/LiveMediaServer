<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:template match="/">
<html>
<body>
<h2>Channel List:</h2>
<table cellspacing="0" cellpading="0" bordercolor="#cccccc" border="1">
<tr bgcolor="#9acd32">
<th align="center">channel_id</th>
<th align="center">liveid</th>
<th align="center">bitrate</th>
<th align="center">channel_name</th>
<th align="center">codec_ts</th>
<th align="center">codec_flv</th>
<th align="center">codec_mp4</th>
<th align="center">source</th>
</tr>

<xsl:for-each select="channels/channel">
<tr>
	<xsl:if test="position() mod 2 = 1">
		<xsl:attribute name="bgcolor">#EEEEEE</xsl:attribute>
	</xsl:if>
<td><xsl:value-of select="@channel_id" /></td>
<td><xsl:value-of select="@liveid" /></td>
<td><xsl:value-of select="@bitrate" /></td>
<td><xsl:value-of select="@channel_name" /></td>
<td><xsl:value-of select="@codec_ts" /></td>
<td><xsl:value-of select="@codec_flv" /></td>
<td><xsl:value-of select="@codec_mp4" /></td>
<td>
	<table cellspacing="0" cellpading="0" frame="void" rules="all" width="200">
	<xsl:for-each select="sources/source">
	<tr>
		<td width="80%" align="left"><xsl:value-of select="@ip" /></td>
		<td width="20%" align="right"><xsl:value-of select="@port" /></td>
	</tr>
	</xsl:for-each>
	</table>
</td>
</tr>

</xsl:for-each>
</table>
</body>
</html>
</xsl:template>

</xsl:stylesheet>
