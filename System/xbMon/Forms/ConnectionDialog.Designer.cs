namespace Atg.Samples.xbMon
{
    partial class ConnectionDialog
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(ConnectionDialog));
            this.label1 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.panel1 = new System.Windows.Forms.Panel();
            this.removeSelectedButton = new System.Windows.Forms.Button();
            this.connectButton = new System.Windows.Forms.Button();
            this.cancelButton = new System.Windows.Forms.Button();
            this.label3 = new System.Windows.Forms.Label();
            this.label4 = new System.Windows.Forms.Label();
            this.consoleTextBox = new System.Windows.Forms.TextBox();
            this.consoleListBox = new System.Windows.Forms.ListBox();
            this.xboxIcon = new System.Windows.Forms.PictureBox();
            this.panel1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.xboxIcon)).BeginInit();
            this.SuspendLayout();
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(9, 20);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(223, 13);
            this.label1.TabIndex = 0;
            this.label1.Text = "&Enter the name or IP address of the console:";
            this.label1.TextAlign = System.Drawing.ContentAlignment.BottomLeft;
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(9, 59);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(298, 13);
            this.label2.TabIndex = 2;
            this.label2.Text = "NOTE: Separate multiple entries with a comma or semi-colon.";
            // 
            // panel1
            // 
            this.panel1.Controls.Add(this.removeSelectedButton);
            this.panel1.Controls.Add(this.connectButton);
            this.panel1.Controls.Add(this.cancelButton);
            this.panel1.Dock = System.Windows.Forms.DockStyle.Bottom;
            this.panel1.Location = new System.Drawing.Point(0, 199);
            this.panel1.Name = "panel1";
            this.panel1.Padding = new System.Windows.Forms.Padding(8);
            this.panel1.Size = new System.Drawing.Size(498, 48);
            this.panel1.TabIndex = 6;
            // 
            // removeSelectedButton
            // 
            this.removeSelectedButton.Location = new System.Drawing.Point(12, 12);
            this.removeSelectedButton.Name = "removeSelectedButton";
            this.removeSelectedButton.Size = new System.Drawing.Size(144, 24);
            this.removeSelectedButton.TabIndex = 0;
            this.removeSelectedButton.Text = "&Remove Selected";
            this.removeSelectedButton.UseVisualStyleBackColor = true;
            this.removeSelectedButton.Click += new System.EventHandler(this.removeSelectedButton_Click);
            // 
            // connectButton
            // 
            this.connectButton.DialogResult = System.Windows.Forms.DialogResult.OK;
            this.connectButton.Location = new System.Drawing.Point(288, 12);
            this.connectButton.Name = "connectButton";
            this.connectButton.Size = new System.Drawing.Size(96, 24);
            this.connectButton.TabIndex = 1;
            this.connectButton.Text = "&Add";
            this.connectButton.UseVisualStyleBackColor = true;
            this.connectButton.Click += new System.EventHandler(this.connectButton_Click);
            // 
            // cancelButton
            // 
            this.cancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            this.cancelButton.Location = new System.Drawing.Point(390, 12);
            this.cancelButton.Name = "cancelButton";
            this.cancelButton.Size = new System.Drawing.Size(96, 24);
            this.cancelButton.TabIndex = 2;
            this.cancelButton.Text = "Cancel";
            this.cancelButton.UseVisualStyleBackColor = true;
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(23, 180);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(226, 13);
            this.label3.TabIndex = 7;
            this.label3.Text = "marks consoles from Xbox 360 Neighborhood.";
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Font = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label4.Location = new System.Drawing.Point(255, 181);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(137, 13);
            this.label4.TabIndex = 7;
            this.label4.Text = "Default console in bold.";
            // 
            // consoleTextBox
            // 
            this.consoleTextBox.Location = new System.Drawing.Point(12, 36);
            this.consoleTextBox.Multiline = true;
            this.consoleTextBox.Name = "consoleTextBox";
            this.consoleTextBox.Size = new System.Drawing.Size(474, 20);
            this.consoleTextBox.TabIndex = 1;
            this.consoleTextBox.WordWrap = false;
            this.consoleTextBox.TextChanged += new System.EventHandler(this.consoleTextBox_TextChanged);
            this.consoleTextBox.Validating += new System.ComponentModel.CancelEventHandler(this.consoleTextBox_Validating);
            // 
            // consoleListBox
            // 
            this.consoleListBox.DrawMode = System.Windows.Forms.DrawMode.OwnerDrawFixed;
            this.consoleListBox.FormattingEnabled = true;
            this.consoleListBox.ItemHeight = 14;
            this.consoleListBox.Location = new System.Drawing.Point(12, 75);
            this.consoleListBox.MultiColumn = true;
            this.consoleListBox.Name = "consoleListBox";
            this.consoleListBox.ScrollAlwaysVisible = true;
            this.consoleListBox.SelectionMode = System.Windows.Forms.SelectionMode.MultiSimple;
            this.consoleListBox.Size = new System.Drawing.Size(474, 102);
            this.consoleListBox.Sorted = true;
            this.consoleListBox.TabIndex = 5;
            this.consoleListBox.MouseDoubleClick += new System.Windows.Forms.MouseEventHandler(this.consoleListBox_MouseDoubleClick);
            this.consoleListBox.DrawItem += new System.Windows.Forms.DrawItemEventHandler(this.consoleListBox_DrawItem);
            this.consoleListBox.MeasureItem += new System.Windows.Forms.MeasureItemEventHandler(this.consoleListBox_MeasureItem);
            this.consoleListBox.SelectedIndexChanged += new System.EventHandler(this.consoleListBox_SelectedIndexChanged);
            // 
            // xboxIcon
            // 
            this.xboxIcon.Image = ((System.Drawing.Image)(resources.GetObject("xboxIcon.Image")));
            this.xboxIcon.Location = new System.Drawing.Point(12, 180);
            this.xboxIcon.Name = "xboxIcon";
            this.xboxIcon.Size = new System.Drawing.Size(14, 14);
            this.xboxIcon.SizeMode = System.Windows.Forms.PictureBoxSizeMode.StretchImage;
            this.xboxIcon.TabIndex = 8;
            this.xboxIcon.TabStop = false;
            // 
            // ConnectionDialog
            // 
            this.AcceptButton = this.connectButton;
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.CancelButton = this.cancelButton;
            this.ClientSize = new System.Drawing.Size(498, 247);
            this.Controls.Add(this.xboxIcon);
            this.Controls.Add(this.label4);
            this.Controls.Add(this.label3);
            this.Controls.Add(this.consoleListBox);
            this.Controls.Add(this.consoleTextBox);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.label1);
            this.Controls.Add(this.panel1);
            this.Font = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "ConnectionDialog";
            this.ShowIcon = false;
            this.ShowInTaskbar = false;
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "Add Console";
            this.Load += new System.EventHandler(this.ConnectionDialog_Load);
            this.panel1.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.xboxIcon)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Button removeSelectedButton;
        private System.Windows.Forms.Button connectButton;
        private System.Windows.Forms.Button cancelButton;
        private System.Windows.Forms.TextBox consoleTextBox;
        private System.Windows.Forms.ListBox consoleListBox;
        private System.Windows.Forms.PictureBox xboxIcon;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Panel panel1;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.Label label4;
    }
}