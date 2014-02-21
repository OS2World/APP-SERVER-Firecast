<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="2.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<!-- Page title -->

<xsl:template name="page-title">
  <xsl:param name="image" />
  <xsl:param name="text" />

  <h1>
    <img height="64" width="64" align="absmiddle">
      <xsl:attribute name="src">/img/<xsl:value-of select="$image" />.png</xsl:attribute>
    </img>
    <xsl:value-of select="$text" />
  </h1>
</xsl:template>


<!-- Buttons -->

<xsl:template name="button-image">
  <xsl:param name="image" />
  <xsl:param name="class" />
  <xsl:param name="text" />
  <xsl:param name="code" />
  <xsl:param name="href" />

  <a>
    <xsl:if test="$code">
      <xsl:attribute name="onclick"><xsl:value-of select="$code" /></xsl:attribute>
    </xsl:if>
    <xsl:attribute name="href">/<xsl:value-of select="$href" /></xsl:attribute>
    <img align="absmiddle"
     onmouseover="this.style.backgroundColor='#F0FfA0';"
     onmouseout="this.style.backgroundColor='';">
      <xsl:attribute name="class"><xsl:value-of select="$class" /></xsl:attribute>
      <xsl:attribute name="src">/img/<xsl:value-of select="$image" /></xsl:attribute>
      <xsl:attribute name="alt"><xsl:value-of select="$text" /></xsl:attribute>
      <xsl:attribute name="title"><xsl:value-of select="$text" /></xsl:attribute>
    </img>
  </a>
</xsl:template>

<xsl:template name="button-image-big">
  <xsl:param name="image" />
  <xsl:param name="text" />
  <xsl:param name="code" />
  <xsl:param name="href" />

  <xsl:call-template name="button-image">
    <xsl:with-param name="image"><xsl:value-of select="$image" /></xsl:with-param>
    <xsl:with-param name="text"><xsl:value-of select="$text" /></xsl:with-param>
    <xsl:with-param name="code"><xsl:value-of select="$code" /></xsl:with-param>
    <xsl:with-param name="href"><xsl:value-of select="$href" /></xsl:with-param>
    <xsl:with-param name="class">btn16x15</xsl:with-param>
  </xsl:call-template>
</xsl:template>

<xsl:template name="button-image-small">
  <xsl:param name="image" />
  <xsl:param name="text" />
  <xsl:param name="code" />
  <xsl:param name="href" />

  <xsl:call-template name="button-image">
    <xsl:with-param name="image"><xsl:value-of select="$image" /></xsl:with-param>
    <xsl:with-param name="text"><xsl:value-of select="$text" /></xsl:with-param>
    <xsl:with-param name="code"><xsl:value-of select="$code" /></xsl:with-param>
    <xsl:with-param name="href"><xsl:value-of select="$href" /></xsl:with-param>
    <xsl:with-param name="class">btn12x11</xsl:with-param>
  </xsl:call-template>
</xsl:template>


<!-- Access list -->

<xsl:template name="access-list">
  <div name="access">
    <xsl:apply-templates select="access/acl" />
    <xsl:call-template name="acl-item" />
  </div>
</xsl:template>

<xsl:template match="acl">
  <xsl:call-template name="acl-item">
    <xsl:with-param name="action" select="@action" />
    <xsl:with-param name="value" select="." />
  </xsl:call-template>
</xsl:template>

<xsl:template name="acl-item">
  <xsl:param name="action" />
  <xsl:param name="value" />

  <div name="acl" class="acl-item">
    <select>
      <option value="allow">allow</option>
      <option value="deny">
        <xsl:if test="$action='deny'">
          <xsl:attribute name="selected">1</xsl:attribute>
        </xsl:if>
        deny
      </option>
    </select>
    <input type="text" style="width: 15em;">
      <xsl:attribute name="value"><xsl:value-of select="$value" /></xsl:attribute>
    </input>

    <xsl:call-template name="button-image-small">
      <xsl:with-param name="image">btn-x.gif</xsl:with-param>
      <xsl:with-param name="text">remove item</xsl:with-param>
      <xsl:with-param name="code">return ItemDelete(this.parentNode);</xsl:with-param>
    </xsl:call-template>
    <xsl:call-template name="button-image-small">
      <xsl:with-param name="image">btn-add.gif</xsl:with-param>
      <xsl:with-param name="text">copy item</xsl:with-param>
      <xsl:with-param name="code">return ItemAdd(this.parentNode);</xsl:with-param>
    </xsl:call-template>
    <xsl:call-template name="button-image-small">
      <xsl:with-param name="image">btn-up.gif</xsl:with-param>
      <xsl:with-param name="text">Up</xsl:with-param>
      <xsl:with-param name="code">return ItemMoveUp(this.parentNode);</xsl:with-param>
    </xsl:call-template>
    <xsl:call-template name="button-image-small">
      <xsl:with-param name="image">btn-dn.gif</xsl:with-param>
      <xsl:with-param name="text">Down</xsl:with-param>
      <xsl:with-param name="code">return ItemMoveDown(this.parentNode);</xsl:with-param>
    </xsl:call-template>
  </div>
</xsl:template>

</xsl:stylesheet>