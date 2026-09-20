namespace Microsoft.ATG.ManagedNUIConduit
{
    partial class MainForm
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
            System.Windows.Forms.Label label1;
            System.Windows.Forms.Label label2;
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(MainForm));
            System.Windows.Forms.Label label3;
            System.Windows.Forms.Label label4;
            this.showConsumerButton = new System.Windows.Forms.Button();
            this.showRecordPlaybackButton = new System.Windows.Forms.Button();
            label1 = new System.Windows.Forms.Label();
            label2 = new System.Windows.Forms.Label();
            label3 = new System.Windows.Forms.Label();
            label4 = new System.Windows.Forms.Label();
            this.SuspendLayout();
            // 
            // label1
            // 
            label1.AutoSize = true;
            label1.Font = new System.Drawing.Font("Segoe UI", 9.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            label1.Location = new System.Drawing.Point(12, 9);
            label1.Name = "label1";
            label1.Size = new System.Drawing.Size(70, 17);
            label1.TabIndex = 0;
            label1.Text = "Consumer";
            // 
            // label2
            // 
            label2.Location = new System.Drawing.Point(12, 26);
            label2.Name = "label2";
            label2.Size = new System.Drawing.Size(340, 71);
            label2.TabIndex = 1;
            label2.Text = resources.GetString("label2.Text");
            // 
            // label3
            // 
            label3.AutoSize = true;
            label3.Font = new System.Drawing.Font("Segoe UI", 9.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            label3.Location = new System.Drawing.Point(12, 127);
            label3.Name = "label3";
            label3.Size = new System.Drawing.Size(110, 17);
            label3.TabIndex = 3;
            label3.Text = "Record/Playback";
            // 
            // label4
            // 
            label4.Location = new System.Drawing.Point(12, 144);
            label4.Name = "label4";
            label4.Size = new System.Drawing.Size(340, 29);
            label4.TabIndex = 4;
            label4.Text = "This demonstrates recording data to and playing back data from a temporary event " +
                "data (XED) file on the local machine.";
            // 
            // showConsumerButton
            // 
            this.showConsumerButton.Location = new System.Drawing.Point(192, 100);
            this.showConsumerButton.Name = "showConsumerButton";
            this.showConsumerButton.Size = new System.Drawing.Size(158, 23);
            this.showConsumerButton.TabIndex = 2;
            this.showConsumerButton.Text = "Show Consumer";
            this.showConsumerButton.UseVisualStyleBackColor = true;
            this.showConsumerButton.Click += new System.EventHandler(this.showConsumerButton_Click);
            // 
            // showRecordPlaybackButton
            // 
            this.showRecordPlaybackButton.Location = new System.Drawing.Point(192, 176);
            this.showRecordPlaybackButton.Name = "showRecordPlaybackButton";
            this.showRecordPlaybackButton.Size = new System.Drawing.Size(158, 23);
            this.showRecordPlaybackButton.TabIndex = 5;
            this.showRecordPlaybackButton.Text = "Show Record/Playback";
            this.showRecordPlaybackButton.UseVisualStyleBackColor = true;
            this.showRecordPlaybackButton.Click += new System.EventHandler(this.showRecordPlaybackButton_Click);
            // 
            // MainForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(362, 212);
            this.Controls.Add(this.showRecordPlaybackButton);
            this.Controls.Add(this.showConsumerButton);
            this.Controls.Add(label4);
            this.Controls.Add(label2);
            this.Controls.Add(label3);
            this.Controls.Add(label1);
            this.Font = new System.Drawing.Font("Segoe UI", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.MaximizeBox = false;
            this.Name = "MainForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "Managed NUI Conduit";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Button showConsumerButton;
        private System.Windows.Forms.Button showRecordPlaybackButton;

    }
}