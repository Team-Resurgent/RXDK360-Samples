namespace Atg.Samples.xbMon
{
    partial class RebootConsole
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
            this.OK = new System.Windows.Forms.Button();
            this.Cancel = new System.Windows.Forms.Button();
            this.RebootTargets = new System.Windows.Forms.ComboBox();
            this.ColdReboot = new System.Windows.Forms.CheckBox();
            this.label1 = new System.Windows.Forms.Label();
            this.CommandLineArguments = new System.Windows.Forms.TextBox();
            this.SuspendLayout();
            // 
            // OK
            // 
            this.OK.DialogResult = System.Windows.Forms.DialogResult.OK;
            this.OK.Location = new System.Drawing.Point(12, 93);
            this.OK.Name = "OK";
            this.OK.Size = new System.Drawing.Size(75, 23);
            this.OK.TabIndex = 3;
            this.OK.Text = "&Ok";
            this.OK.UseVisualStyleBackColor = true;
            // 
            // Cancel
            // 
            this.Cancel.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.Cancel.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            this.Cancel.Location = new System.Drawing.Point(300, 93);
            this.Cancel.Name = "Cancel";
            this.Cancel.Size = new System.Drawing.Size(75, 23);
            this.Cancel.TabIndex = 4;
            this.Cancel.Text = "&Cancel";
            this.Cancel.UseVisualStyleBackColor = true;
            // 
            // RebootTargets
            // 
            this.RebootTargets.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.RebootTargets.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.RebootTargets.FormattingEnabled = true;
            this.RebootTargets.Location = new System.Drawing.Point(12, 12);
            this.RebootTargets.Name = "RebootTargets";
            this.RebootTargets.Size = new System.Drawing.Size(363, 21);
            this.RebootTargets.TabIndex = 0;
            // 
            // ColdReboot
            // 
            this.ColdReboot.AutoSize = true;
            this.ColdReboot.CheckAlign = System.Drawing.ContentAlignment.MiddleRight;
            this.ColdReboot.Location = new System.Drawing.Point(71, 65);
            this.ColdReboot.Name = "ColdReboot";
            this.ColdReboot.Size = new System.Drawing.Size(85, 17);
            this.ColdReboot.TabIndex = 2;
            this.ColdReboot.Text = "Cold Reboot";
            this.ColdReboot.UseVisualStyleBackColor = true;
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(12, 42);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(129, 13);
            this.label1.TabIndex = 4;
            this.label1.Text = "Command-line Arguments:";
            // 
            // CommandLineArguments
            // 
            this.CommandLineArguments.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.CommandLineArguments.Location = new System.Drawing.Point(141, 39);
            this.CommandLineArguments.Name = "CommandLineArguments";
            this.CommandLineArguments.Size = new System.Drawing.Size(234, 20);
            this.CommandLineArguments.TabIndex = 1;
            // 
            // RebootConsole
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(380, 128);
            this.Controls.Add(this.CommandLineArguments);
            this.Controls.Add(this.label1);
            this.Controls.Add(this.ColdReboot);
            this.Controls.Add(this.RebootTargets);
            this.Controls.Add(this.Cancel);
            this.Controls.Add(this.OK);
            this.Icon = global::Atg.Samples.xbMon.Resources.Resources.SystemTrayIcon;
            this.Name = "RebootConsole";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "RebootConsole";
            this.Load += new System.EventHandler(this.RebootConsole_Load);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Button OK;
        private System.Windows.Forms.Button Cancel;
        private System.Windows.Forms.ComboBox RebootTargets;
        private System.Windows.Forms.CheckBox ColdReboot;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.TextBox CommandLineArguments;
    }
}