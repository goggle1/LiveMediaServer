<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:template match="/">
<html>
<body>
<h2>Channel Status:</h2>
<xsl:apply-templates select="channels"/> 
</body>
</html>
</xsl:template>

<xsl:template match="channels">
<table cellspacing="0" cellpading="0" bordercolor="#cccccc" border="1">
<xsl:for-each select="channel">
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
<tr bgcolor="#EEEEEE">		
	<td><xsl:value-of select="@channel_id" /></td>
	<td><xsl:value-of select="@liveid" /></td>
	<td><xsl:value-of select="@bitrate" /></td>
	<td><xsl:value-of select="@channel_name" /></td>
	<td><xsl:value-of select="@codec_ts" /></td>
	<td><xsl:value-of select="@codec_flv" /></td>
	<td><xsl:value-of select="@codec_mp4" /></td>
	<td>
		<xsl:apply-templates select="sources"/>	
	</td>	
</tr>
<xsl:apply-templates select="status"/>
</xsl:for-each>
</table>
</xsl:template>

<xsl:template match="sources">
<table cellspacing="0" cellpading="0" frame="void" rules="all" width="100%">
	<tr bgcolor="#9acd32">
	<th align="center">ip</th>
	<th align="center">port</th>
	</tr>
	<xsl:for-each select="source">
	<tr>
		<td width="80%" align="left"><xsl:value-of select="@ip" /></td>
		<td width="20%" align="right"><xsl:value-of select="@port" /></td>
	</tr>
	</xsl:for-each>
</table>
</xsl:template>

<xsl:template match="status">
<tr>
	<td colspan="1">
	</td>
	<td colspan="7">
	
		<table cellspacing="0" cellpading="0" frame="void" rules="all" width="1600">	
			<tr bgcolor="#9acd32">
				<th align="center">stream</th>
				<th align="center">source</th>
				<th align="center">m3u8_num</th>
				<th align="center">clip_num</th>
				<th align="center">m3u8_begin_time</th>
				<th align="center">m3u8_end_time</th>
				<th align="center">clip_begin_time</th>
				<th align="center">clip_begin_time</th>
			</tr>	
			
			<xsl:apply-templates select="tss"/>
			
			<xsl:apply-templates select="flv"/>
			 
			<xsl:apply-templates select="mp4"/>
		
		</table>
	
	</td>	
</tr>
</xsl:template>

<xsl:template match="tss">
<tr>
	<td> ts </td>
	<td><xsl:value-of select="@source" /></td>
	<td><xsl:value-of select="@m3u8_num" /></td>
	<td><xsl:value-of select="@clip_num" /></td>
	<td><xsl:value-of select="@m3u8_begin_time" /></td>
	<td><xsl:value-of select="@m3u8_end_time" /></td>
	<td><xsl:value-of select="@clip_begin_time" /></td>
	<td><xsl:value-of select="@clip_begin_time" /></td>
</tr>	
</xsl:template>

<xsl:template match="flv">
<tr>
	<td> flv </td>
	<td><xsl:value-of select="@source" /></td>
	<td><xsl:value-of select="@m3u8_num" /></td>
	<td><xsl:value-of select="@clip_num" /></td>
	<td><xsl:value-of select="@m3u8_begin_time" /></td>
	<td><xsl:value-of select="@m3u8_end_time" /></td>
	<td><xsl:value-of select="@clip_begin_time" /></td>
	<td><xsl:value-of select="@clip_begin_time" /></td>
</tr>
</xsl:template>

<xsl:template match="mp4">
<tr>
	<td> mp4 </td>
	<td><xsl:value-of select="@source" /></td>
	<td><xsl:value-of select="@m3u8_num" /></td>
	<td><xsl:value-of select="@clip_num" /></td>
	<td><xsl:value-of select="@m3u8_begin_time" /></td>
	<td><xsl:value-of select="@m3u8_end_time" /></td>
	<td><xsl:value-of select="@clip_begin_time" /></td>
	<td><xsl:value-of select="@clip_begin_time" /></td>
</tr>
</xsl:template>

</xsl:stylesheet>

