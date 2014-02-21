<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="2.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="html" encoding="utf-8" indent="yes"/>

<xsl:include href="utils.xsl" />

<xsl:template match="/">
<head>
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
  <meta http-equiv="Content-Script-Type" content="text/javascript" />
  <script type="text/javascript" src="utils.js"></script>
  <script type="text/javascript" src="templates.js"></script>
  <link rel="stylesheet" type="text/css" href="style.css" />
</head>
<body>
  <xsl:call-template name="page-title">
    <xsl:with-param name="image">templates</xsl:with-param>
    <xsl:with-param name="text">Templates</xsl:with-param>
  </xsl:call-template>

  <div id="templates-list">
    <xsl:apply-templates select="templates/template" />
  </div>

  <form enctype="text/plain" action="/admin/templates"
        method="post" name="apply"
        onsubmit="
           document.apply.XMLText.value = TemplatesMakeXML( document.getElementById('templates-list') );
           return true;">
    <input type="hidden" name="XMLText" />
    <input type="submit" value="Apply" />
  </form>
</body>
</xsl:template>

<xsl:template match="template">
  <div name="template" class="block">
    <xsl:call-template name="button-image-big">
      <xsl:with-param name="image">btn-big-x.gif</xsl:with-param>
      <xsl:with-param name="text">remove template</xsl:with-param>
      <xsl:with-param name="code">return ItemDelete(this.parentNode);</xsl:with-param>
    </xsl:call-template>
    <xsl:call-template name="button-image-big">
      <xsl:with-param name="image">btn-big-add.gif</xsl:with-param>
      <xsl:with-param name="text">new template</xsl:with-param>
      <xsl:with-param name="code">return ItemAdd(this.parentNode);</xsl:with-param>
    </xsl:call-template>
    <xsl:call-template name="button-image-big">
      <xsl:with-param name="image">btn-big-up.gif</xsl:with-param>
      <xsl:with-param name="text">Up</xsl:with-param>
      <xsl:with-param name="code">return ItemMoveUp(this.parentNode);</xsl:with-param>
    </xsl:call-template>
    <xsl:call-template name="button-image-big">
      <xsl:with-param name="image">btn-big-dn.gif</xsl:with-param>
      <xsl:with-param name="text">Down</xsl:with-param>
      <xsl:with-param name="code">return ItemMoveDown(this.parentNode);</xsl:with-param>
    </xsl:call-template>

    <label>
      <input type="checkbox" name="denided">
        <xsl:if test="@denided=1">
          <xsl:attribute name="checked">1</xsl:attribute>
        </xsl:if>
      </input>
      denided
    </label>
    <label>
      <input type="checkbox" name="hidden">
        <xsl:if test="@hidden=1">
          <xsl:attribute name="checked">1</xsl:attribute>
        </xsl:if>
      </input>
      hidden
    </label>
    <br />

    <span class="field">Mount point mask:</span>
    <input type="text" name="mount-point">
      <xsl:attribute name="value">
        <xsl:value-of select="mount-point" />
      </xsl:attribute>
    </input>

    <h2>Access list:</h2>
    <xsl:call-template name="access-list" />

    <span class="field">Password:</span>
    <input type="password" name="password">
      <xsl:attribute name="value">
        <xsl:value-of select="password" />
      </xsl:attribute>
    </input>

    <span class="field" style="margin-left: 1em;">Keep alive:</span>
    <input type="text" name="keep-alive" style="width: 4em;">
      <xsl:attribute name="value">
        <xsl:value-of select="keep-alive" />
      </xsl:attribute>
    </input>
    sec.

    <table border="0" cellspacing="0" cellpadding="0">
      <xsl:call-template name="field-controls">
        <xsl:with-param name="caption">Stream title</xsl:with-param>
        <xsl:with-param name="override" select="field[@name='icy-name']/@override" />
        <xsl:with-param name="name">icy-name</xsl:with-param>
        <xsl:with-param name="value" select="field[@name='icy-name']" />
      </xsl:call-template>
      <xsl:call-template name="field-controls">
        <xsl:with-param name="caption">Stream URL</xsl:with-param>
        <xsl:with-param name="override" select="field[@name='icy-url']/@override" />
        <xsl:with-param name="name">icy-url</xsl:with-param>
        <xsl:with-param name="value" select="field[@name='icy-url']" />
      </xsl:call-template>
      <xsl:call-template name="field-controls">
        <xsl:with-param name="caption">Bitrate [Kb/s]</xsl:with-param>
        <xsl:with-param name="override" select="field[@name='icy-br']/@override" />
        <xsl:with-param name="name">icy-br</xsl:with-param>
        <xsl:with-param name="value" select="field[@name='icy-br']" />
      </xsl:call-template>
      <xsl:call-template name="field-controls">
        <xsl:with-param name="caption">Music style</xsl:with-param>
        <xsl:with-param name="override" select="field[@name='icy-genre']/@override" />
        <xsl:with-param name="name">icy-genre</xsl:with-param>
        <xsl:with-param name="value" select="field[@name='icy-genre']" />
      </xsl:call-template>
      <xsl:call-template name="field-controls">
        <xsl:with-param name="caption">Description</xsl:with-param>
        <xsl:with-param name="override" select="field[@name='icy-description']/@override" />
        <xsl:with-param name="name">icy-description</xsl:with-param>
        <xsl:with-param name="value" select="field[@name='icy-description']" />
      </xsl:call-template>
      <xsl:call-template name="field-controls">
        <xsl:with-param name="caption">Public (0/1)</xsl:with-param>
        <xsl:with-param name="override" select="field[@name='icy-pub']/@override" />
        <xsl:with-param name="name">icy-pub</xsl:with-param>
        <xsl:with-param name="value" select="field[@name='icy-pub']" />
      </xsl:call-template>
      <tr>
        <td colspan="3">&#xA0;&#xA0;<b>^-</b> override</td>
      </tr>
    </table>

    <h2>Clients:</h2>
    <xsl:apply-templates select="clients" />
  </div>
</xsl:template>

<xsl:template match="clients">
  <div style="margin-left: 1em;" name="clients">
    <span class="field">Max. clients:</span>
    <input type="text" name="max-clients" style="width: 4em;">
      <xsl:attribute name="value">
        <xsl:value-of select="max-clients" />
      </xsl:attribute>
    </input>

    <xsl:call-template name="access-list" />
  </div>
</xsl:template>

<xsl:template name="field-controls">
  <xsl:param name="caption" />
  <xsl:param name="name" />
  <xsl:param name="value" />
  <xsl:param name="override" />

  <tr name="field">
    <td width="1">
      <input type="checkbox">
        <xsl:if test="$override=1">
          <xsl:attribute name="checked">1</xsl:attribute>
        </xsl:if>
      </input>
    </td>

    <td>
      <label>
        <xsl:attribute name="for">
          <xsl:value-of select="$name" />
        </xsl:attribute>
        <span class="field"><xsl:value-of select="$caption" />: </span>
      </label>
    </td>

    <td>
      <input type="text" size="60" maxlength="255">
        <xsl:attribute name="id">
          <xsl:value-of select="$name" />
        </xsl:attribute>
        <xsl:attribute name="name">
          <xsl:value-of select="$name" />
        </xsl:attribute>
        <xsl:attribute name="value">
          <xsl:value-of select="$value" />
        </xsl:attribute>
      </input>
    </td>
  </tr>
</xsl:template>

</xsl:stylesheet>

