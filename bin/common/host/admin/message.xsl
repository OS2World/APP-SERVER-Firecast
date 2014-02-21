<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="2.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="html" encoding="utf-8" indent="yes"/>

<xsl:include href="utils.xsl" />

<xsl:template match="/message">
<head>
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
  <link rel="stylesheet" type="text/css" href="style.css" />
</head>
<body>
  <div class="msg">
    <div class="msg-type">
      <img align="absmiddle">
        <xsl:attribute name="src">/img/msg-<xsl:value-of select="type" />.png</xsl:attribute>
      </img>
      <xsl:value-of select="type" />
    </div>
    <div class="msg-text"><xsl:value-of select="text" /></div>
    <br />
    <xsl:apply-templates select="part" />
  </div>
</body>
</xsl:template>

<xsl:template match="part">
    <xsl:choose>
      <xsl:when test=".='sources'">
          <xsl:choose>
            <xsl:when test="/message/@admin">
              <xsl:call-template name="button-image-back">
                <xsl:with-param name="image">btn-sources.png</xsl:with-param>
                <xsl:with-param name="text">Sources</xsl:with-param>
                <xsl:with-param name="href">sources</xsl:with-param>
              </xsl:call-template>
            </xsl:when>
            <xsl:otherwise>
              <xsl:call-template name="button-image-back">
                <xsl:with-param name="image">btn-sources-info.png</xsl:with-param>
                <xsl:with-param name="text">Sources</xsl:with-param>
                <xsl:with-param name="href">sources-info</xsl:with-param>
              </xsl:call-template>
            </xsl:otherwise>
          </xsl:choose>
      </xsl:when>
      <xsl:when test=".='templates'">
        <xsl:call-template name="button-image-back">
          <xsl:with-param name="image">btn-templates.png</xsl:with-param>
          <xsl:with-param name="text">Templates</xsl:with-param>
          <xsl:with-param name="href">templates</xsl:with-param>
        </xsl:call-template>
      </xsl:when>
      <xsl:when test=".='hosts'">
        <xsl:call-template name="button-image-back">
          <xsl:with-param name="image">btn-hosts.png</xsl:with-param>
          <xsl:with-param name="text">Web-hosts</xsl:with-param>
          <xsl:with-param name="href">hosts</xsl:with-param>
        </xsl:call-template>
      </xsl:when>
      <xsl:when test=".='general'">
        <xsl:call-template name="button-image-back">
          <xsl:with-param name="image">btn-general.png</xsl:with-param>
          <xsl:with-param name="text">General</xsl:with-param>
          <xsl:with-param name="href">general</xsl:with-param>
        </xsl:call-template>
      </xsl:when>
      <xsl:when test=".='admin'">
        <xsl:call-template name="button-image-back">
          <xsl:with-param name="image">btn-admin.png</xsl:with-param>
          <xsl:with-param name="text">Administrator</xsl:with-param>
          <xsl:with-param name="href">admin</xsl:with-param>
        </xsl:call-template>
      </xsl:when>
      <xsl:when test=".='relays'">
        <xsl:call-template name="button-image-back">
          <xsl:with-param name="image">btn-relays.png</xsl:with-param>
          <xsl:with-param name="text">Relays</xsl:with-param>
          <xsl:with-param name="href">relays</xsl:with-param>
        </xsl:call-template>
      </xsl:when>
    </xsl:choose>
</xsl:template>

<xsl:template name="button-image-back">
  <xsl:param name="image" />
  <xsl:param name="text" />
  <xsl:param name="code" />
  <xsl:param name="href" />

  <xsl:call-template name="button-image">
    <xsl:with-param name="image"><xsl:value-of select="$image" /></xsl:with-param>
    <xsl:with-param name="text"><xsl:value-of select="$text" /></xsl:with-param>
    <xsl:with-param name="code"><xsl:value-of select="$code" /></xsl:with-param>
    <xsl:with-param name="href">admin/<xsl:value-of select="$href" /></xsl:with-param>
    <xsl:with-param name="class">btn32x32</xsl:with-param>
  </xsl:call-template>
</xsl:template>

</xsl:stylesheet>
